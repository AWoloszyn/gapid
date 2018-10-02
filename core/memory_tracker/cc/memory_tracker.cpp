/*
 * Copyright (C) 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "memory_tracker.h"

#if COHERENT_TRACKING_ENABLED
#include <map>
#include <cassert>
#include <cstring>
#include <emmintrin.h>  

namespace gapii {
namespace track_memory {

MemoryTracker::derived_tracker_type* unique_tracker = nullptr;

void DirtyPageTable::RecollectIfPossible(size_t num_stale_pages) {
  if (pages_.size() - stored_ > num_stale_pages) {
    // If we have more extra spaces for recollection than required, remove
    // required number of not used spaces.
    size_t new_size = pages_.size() - num_stale_pages;
    pages_.resize(new_size);
  } else {
    // Otherwise shrink the storage to hold all not-yet-dumped dirty pages.
    pages_.resize(stored_ + 1);
  }
}

std::vector<void*> DirtyPageTable::DumpAndClearInRange(void* start,
                                                       size_t size) {
  uintptr_t start_addr = reinterpret_cast<uintptr_t>(start);
  std::vector<void*> r;
  r.reserve(stored_);
  for (auto it = pages_.begin(); it != current_;) {
    auto nt = std::next(it, 1);
    if (*it >= start_addr && *it < start_addr + size) {
      r.push_back(reinterpret_cast<void*>(*it));
      pages_.splice(pages_.end(), pages_, it);
      stored_ -= 1;
    }
    it = nt;
  }
  return r;
}

std::vector<void*> DirtyPageTable::DumpAndClearAll() {
  std::vector<void*> r;
  r.reserve(stored_);
  std::for_each(pages_.begin(), current_, [&r](uintptr_t addr) {
    r.push_back(reinterpret_cast<void*>(addr));
  });
  // Set the space for the next page address to the beginning of the storage,
  // and reset the counter.
  current_ = pages_.begin();
  stored_ = 0u;
  return r;
}

template <>
void MemoryTracker::TrackMappedMemoryImpl(void** _start, size_t size) {
  void* start = _start[0];
  if (size == 0) return;
  if (IsInRanges(reinterpret_cast<uintptr_t>(start), ranges_)) return;

  uintptr_t size_aligned = GetPageEnd(size, page_size_);
  trackable_memory tm = AllocateTrackableMemory(size_aligned);
  void* ret_mem = tm.primary_address;
  uintptr_t retm = reinterpret_cast<uintptr_t>(ret_mem);
  void* cache = malloc(size_aligned);
  assert((retm & (GetPageSize()-1)) == 0);
  memcpy(ret_mem, start, size);
  memcpy(cache, start, size);

  cpu_written_pages_.Reserve(size_aligned / page_size_);
  cpu_read_pages_.Reserve(size_aligned / page_size_);
  ranges_[reinterpret_cast<uintptr_t>(ret_mem)] = range{size, size_aligned, reinterpret_cast<uintptr_t>(start)};
  
  cpu_alloc_info[retm] = allocation_info{start, tm.secondary_address, cache};
  for (uintptr_t i = retm; i < retm + size_aligned; i += GetPageSize()) {
    status_[i] = page_status{PageProtections::kNone};
  }

  set_protection(ret_mem, size_aligned, PageProtections::kNone);
  _start[0] = ret_mem;
}

template <>
bool MemoryTracker::UntrackMappedMemoryImpl(void* start, size_t size) {
  auto gpu_it = cpu_alloc_info.find(reinterpret_cast<uintptr_t>(start));
  if (gpu_it == cpu_alloc_info.end()) {
    return false;
  }

  auto it = ranges_.find(gpu_it->first);
  if (it == ranges_.end()) return false;

  cpu_written_pages_.RecollectIfPossible(it->second.allocatedSize / page_size_);
  cpu_read_pages_.RecollectIfPossible(it->second.allocatedSize / page_size_);
  FreeTrackableMemory(reinterpret_cast<void*>(gpu_it->first), (gpu_it->second.read_write_cpu_addr), it->second.allocatedSize);
  free(gpu_it->second.cache_addr);
  ranges_.erase(it);
  cpu_alloc_info.erase(gpu_it);

  return true;
}

template <>
void MemoryTracker::UntrackAllMappedMemory() {
  for (auto gpu_it = cpu_alloc_info.begin(); gpu_it != cpu_alloc_info.end();) {
    auto it = ranges_.find(gpu_it->first);
    FreeTrackableMemory(reinterpret_cast<void*>(gpu_it->first), (gpu_it->second.read_write_cpu_addr), it->second.allocatedSize);
    free(gpu_it->second.cache_addr);
    ranges_.erase(it);
    gpu_it = cpu_alloc_info.erase(gpu_it);
  }
}

template <>
void MemoryTracker::ResetCPUReadPagesImpl() {
  std::vector<void*> pages = 
    cpu_read_pages_.DumpAndClearAll();

  for (auto& pg: pages) {
    uintptr_t pa = reinterpret_cast<uintptr_t>(pg);
    page_status& s = status_[pa];
    set_protection(pg, page_size_, s.prot ^ PageProtections::kRead);
    s.prot = s.prot ^ PageProtections::kRead;
  }
}

void CopyWithCache(void* dest, void* src, size_t size, void* cache) {
#if defined(__x86_64) || defined(__i386)
  int64_t* d = static_cast<int64_t*>(dest);
  __m128i* s = static_cast<__m128i*>(src);
  int64_t* c = static_cast<int64_t*>(cache);

  for (size_t i = 0; i < size / sizeof(__m128i); ++i) {
    __m128i _s = _mm_load_si128(&s[i]);
    __m128i _c = _mm_load_si128((__m128i*)(&c[2*i]));
    if (0xFFFF != _mm_movemask_epi8(_mm_cmpeq_epi8(_s, _c))) {
      __m128i t = _mm_xor_si128(_s, _c);
      alignas(16) int64_t v[2];
      _mm_store_si128((__m128i*)(&c[2*i]), _s);
      _mm_store_si128((__m128i*)(&v), t);
      d[2*i] ^= v[0];
      d[2*i + 1] ^= v[1];
    }
  }
#else
  int64_t* d = static_cast<int64_t*>(dest);
  int64_t* s = static_cast<int64_t*>(src);
  int64_t* c = static_cast<int64_t*>(cache);

  for (size_t i = 0; i < size / sizeof(int64_t); ++i) {
    int64_t t = s[i] ^ c[i];
    c[i] ^= t;
    d[i] ^= t;
  }
#endif
}

template <>
void MemoryTracker::ForeachWrittenCPUPageImpl(std::function<void(uintptr_t, uintptr_t)> cb) {
  std::vector<void*> pages = 
    cpu_written_pages_.DumpAndClearAll();

  for (auto& pg: pages) {
    uintptr_t pa = reinterpret_cast<uintptr_t>(pg);
    page_status& s = status_[pa];

    auto it = cpu_alloc_info.upper_bound(pa);
    it--;
    uintptr_t cpu_offs = pa - it->first;
    void* gpuAddr = reinterpret_cast<uint8_t*>(it->second.gpu_addr) + cpu_offs;
    void* cpuAddr = reinterpret_cast<uint8_t*>(it->second.read_write_cpu_addr) + cpu_offs;
    void* cacheAddr = reinterpret_cast<uint8_t*>(it->second.cache_addr) + cpu_offs;

    set_protection(pg, page_size_, s.prot ^ PageProtections::kWrite);
    cb(pa, reinterpret_cast<uintptr_t>(cpuAddr));
    CopyWithCache(gpuAddr, cpuAddr, page_size_, cacheAddr);
    s.prot = s.prot ^ PageProtections::kWrite;
  }
}

template <>
bool MemoryTracker::HandleSegfaultImpl(void* fault_addr, PageProtections faultType) {
  if (!IsInRanges(reinterpret_cast<uintptr_t>(fault_addr), ranges_, true)) {
    return false;
  }

  // The fault address is within a tracking range
  void* page_addr = GetAlignedAddress(fault_addr, page_size_);
  uintptr_t pa = reinterpret_cast<uintptr_t>(page_addr);
  page_status& s = status_[pa];
  
  // We have a read/write page, and we got a fault. That is wrong
  if (s.prot == PageProtections::kReadWrite) {
    return false;
  }

  if ((s.prot & faultType) != PageProtections::kNone) {
    // We are falling into here because we have mis-protected a page
    // previously, so free anything we dont have
    faultType = PageProtections::kReadWrite ^ s.prot;
  }


  auto it = cpu_alloc_info.upper_bound(pa);
  it--;

  uintptr_t cpu_offs = pa - it->first;
  void* gpuAddr =  reinterpret_cast<uint8_t*>(it->second.gpu_addr) + cpu_offs;
  void* cpuAddr = reinterpret_cast<uint8_t*>(it->second.read_write_cpu_addr) + cpu_offs;
  void* cacheAddr = reinterpret_cast<uint8_t*>(it->second.cache_addr) + cpu_offs;

  faultType = faultType | s.prot;
  s.prot = faultType;
  set_protection(page_addr, page_size_, faultType);

  if ((faultType & PageProtections::kRead) != PageProtections::kNone) {
    if (!cpu_read_pages_.Record(page_addr)) {
      return false;
    }
    CopyWithCache(cpuAddr, gpuAddr, page_size_, cacheAddr);
  }

  if ((faultType & PageProtections::kWrite) != PageProtections::kNone) {
    if (!cpu_written_pages_.Record(page_addr)) {
      return false;
    }
  }
  
  return true;
}

}  // namespace track_memory
}  // namespace gapii

#endif  // COHERENT_TRACKING_ENABLED
