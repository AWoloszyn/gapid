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

#include "core/cc/target.h"

#ifndef GAPII_MEMORY_TRACKER_H
#define GAPII_MEMORY_TRACKER_H

#define COHERENT_TRACKING_ENABLED 1
#if COHERENT_TRACKING_ENABLED
#if (TARGET_OS == GAPID_OS_LINUX) || (TARGET_OS == GAPID_OS_ANDROID)
#define IS_POSIX 1
#include "posix/memory_tracker.h"
#elif (TARGET_OS == GAPID_OS_WINDOWS)
#define IS_POSIX 0
#include "windows/memory_tracker.h"
#else
#undef COHERENT_TRACKING_ENABLED
#define COHERENT_TRACKING_ENABLED 0
#endif
#endif  // COHERENT_TRACKING_ENABLED

#if COHERENT_TRACKING_ENABLED

#include <algorithm>
#include <atomic>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <utility>
#include <vector>
#include <unordered_map>

#include "core/memory_tracker/cc/memory_protections.h"

namespace gapii {
namespace track_memory {

// Returns the lower bound aligned address for a given pointer |addr| and a
// given |alignment|. |alignment| must be a power of two and cannot be zero.
// If the given |alignment| is zero or not a power of two, the return is
// undefined.
inline void* GetAlignedAddress(void* addr, size_t alignment) {
  return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(addr) &
                                 ~(alignment - 1u));
}

// Returns the end of the page that is >= addr.
inline uintptr_t GetPageEnd(uintptr_t addr, size_t pagesize) {
  return ((addr + pagesize) & ~(pagesize - 1u));
}


// For a given memory range specified by staring address: |addr| and size:
// |size_unaligned|, returns the size of memory space occupied by the range in
// term of |alignment| aligned memory space.
// e.g.: staring address: 0x5, size_unaligned: 0x7, alignment: 0x8
//       => occupied range in term of aligned memory: 0x0 ~ 0x10
//       => size in aligned memory: 0x10
inline size_t GetAlignedSize(void* addr, size_t size_unaligned,
                             size_t alignment) {
  if (alignment == 0 || size_unaligned == 0) return 0;
  void* start_addr_aligned = GetAlignedAddress(addr, alignment);
  uintptr_t end_across_boundry =
      reinterpret_cast<uintptr_t>(addr) + size_unaligned + alignment - 1;
  if (end_across_boundry < reinterpret_cast<uintptr_t>(addr) ||
      end_across_boundry < size_unaligned ||
      end_across_boundry < alignment - 1) {
    // Overflow
    return 0;
  }
  void* end_addr_aligned =
      GetAlignedAddress(reinterpret_cast<void*>(end_across_boundry), alignment);
  return reinterpret_cast<uintptr_t>(end_addr_aligned) -
         reinterpret_cast<uintptr_t>(start_addr_aligned);
}

// SpinLock is a spin lock implemented with atomic variable and opertions.
// Mutiple calls to Lock in a single thread will result into a deadlock.
class SpinLock {
 public:
  SpinLock() : var_(0) {}
  // Lock acquires the lock.
  void Lock() {
    uint32_t l = kUnlocked;
    while (!var_.compare_exchange_strong(l, kLocked)) {
      l = kUnlocked;
    }
  }
  // Unlock releases the lock.
  void Unlock() { var_.exchange(kUnlocked); }

 private:
  std::atomic<uint32_t> var_;
  const uint32_t kLocked = 1u;
  const uint32_t kUnlocked = 0u;
};

// SpinLockGuard acquires the specified spin lock when it is constructed and
// release the lock when it is destroyed.
class SpinLockGuard {
 public:
  SpinLockGuard(SpinLock* l) : l_(l) {
    if (l_) {
      l_->Lock();
    }
  }
  ~SpinLockGuard() {
    if (l_) {
      l_->Unlock();
    }
  }
  // Not copyable, not movable.
  SpinLockGuard(const SpinLockGuard&) = delete;
  SpinLockGuard(SpinLockGuard&&) = delete;
  SpinLockGuard& operator=(const SpinLockGuard&) = delete;
  SpinLockGuard& operator=(SpinLockGuard&&) = delete;

 private:
  SpinLock* l_;
};

// SpinLockGuarded wraps a given non-static class member function with the
// creation of a SpinLockGuard object. Suppose a class: |Task|, object: |task|,
// a non-static member function: |Do(int)|, and a spin lock: |spin_lock|, the
// wrapper should be used in the following way:
//   auto wrapped = SpinLockGuarded<Task, decltype(Task::Do)>(
//      &task, &Task::Do, &spin_lock);
template <typename OwnerTy, typename MemberFuncPtrTy>
class SpinLockGuarded {
 public:
  SpinLockGuarded(OwnerTy* o, const MemberFuncPtrTy& f, SpinLock* l)
      : owner_(o), f_(f), l_(l) {}

  template <typename... Args>
  auto operator()(Args&&... args) ->
      typename std::result_of<MemberFuncPtrTy(OwnerTy*, Args&&...)>::type {
    SpinLockGuard g(l_);
    return ((*owner_).*f_)(std::forward<Args>(args)...);
  }

 protected:
  OwnerTy* owner_;
  MemberFuncPtrTy f_;
  SpinLock* l_;
};

// SignalSafe wraps a given non-static class member function with the creation
// of a SignalBlocker object then a SpinLockGuard object. Suppose a class:
// |Task|, object: |task|, a non-static member function: |Do(int)|, a signal to
// block: |signal_value|, and a spin lock: |spin_lock|, the wrapper should be
// used in the following way:
//   auto wrapped = SignalSafe<Task, decltype(Task::Do)>(
//      &task, &Task::Do, &spin_lock, signal_value);
template <typename OwnerTy, typename MemberFuncPtrTy>
class SignalSafe : public SpinLockGuarded<OwnerTy, MemberFuncPtrTy> {
  using SpinLockGuardedFunctor = SpinLockGuarded<OwnerTy, MemberFuncPtrTy>;

 public:
  SignalSafe(OwnerTy* o, const MemberFuncPtrTy& f, SpinLock* l, int sig)
      : SpinLockGuarded<OwnerTy, MemberFuncPtrTy>(o, f, l), sig_(sig) {}

  template <typename... Args>
  auto operator()(Args&&... args) ->
      typename std::result_of<MemberFuncPtrTy(OwnerTy*, Args&&...)>::type {
    SignalBlocker g(sig_);
    return this->SpinLockGuardedFunctor::template operator()<Args...>(
        std::forward<Args>(args)...);
  }

 protected:
  int sig_;
};

// DirtyPageTable holds the addresses of dirty memory pages. It pre-allocates
// its storage space for recording. Recording the space for new dirty pages
// will not acquire new memory space.
class DirtyPageTable {
 public:
  DirtyPageTable() : stored_(0u), pages_(1u), current_(pages_.begin()) {}

  // Not copyable, not movable.
  DirtyPageTable(const DirtyPageTable&) = delete;
  DirtyPageTable(DirtyPageTable&&) = delete;
  DirtyPageTable& operator=(const DirtyPageTable&) = delete;
  DirtyPageTable& operator=(DirtyPageTable&&) = delete;

  ~DirtyPageTable() {}

  // Record records the given address to the next storage space if such a space
  // is available, increases the counter of stored addresses then returns true.
  // If such a space is not available, returns false without trying to record
  // the address. Record does not check whether the given page_addr has been
  // already recorded before.
  bool Record(void* page_addr) {
    if (std::next(current_, 1) == pages_.end()) {
      return false;
    }
    *current_ = reinterpret_cast<uintptr_t>(page_addr);
    stored_++;
    current_++;
    return true;
  }

  // Has returns true if the given page_addr has already been recorded and not
  // yet dumpped, otherwise returns false;
  bool Has(void* page_addr) const {
    for (auto i = pages_.begin(); i != current_; i++) {
      if (reinterpret_cast<uintptr_t>(page_addr) == *i) {
        return true;
      }
    }
    return false;
  }

  // Reserve reservers the spaces for recording |num_new_pages| number of
  // pages.
  void Reserve(size_t num_new_pages) {
    pages_.resize(pages_.size() + num_new_pages, 0);
  }

  // RecollectIfPossible tries to recollect the space used for recording
  // |num_stale_pages| number of pages. If there are fewer not-used spaces than
  // specified, it shrinks the storage to hold just enough space for recorded
  // dirty pages, or at least one page when no page has been recorded.
  void RecollectIfPossible(size_t num_stale_pages);

  // DumpAndClearInRange dumps the recorded page addresses within a specified
  // range, starting from |start| with |size| large, to a std::vector, clears
  // the internal records without releasing the spaces and returns the page
  // address vector.
  std::vector<void*> DumpAndClearInRange(void* start, size_t size);

  // DumpAndClearInRange dumps all the recorded page addresses to a
  // std::vector, clears the internal records without releasing the spaces and
  // returns the page address vector.
  std::vector<void*> DumpAndClearAll();

 protected:
  size_t stored_;  // A counter for the number of page addresses stored.
  std::list<uintptr_t> pages_;  // Internal storage of the page addresses.
  std::list<uintptr_t>::iterator
      current_;  // The space for the last page address stored.
};

// MemoryTrackerImpl utilizes Segfault signal on Linux to track accesses to
// memories.
template <typename SpecificMemoryTracker>
class MemoryTrackerImpl : public SpecificMemoryTracker {
 public:
  using derived_tracker_type = SpecificMemoryTracker;
  // Creates a memory tracker to track memory write operations. If
  // |track_read| is set to true, also tracks the memory read operations.
  // By default |track_read| is set to false.
  MemoryTrackerImpl(bool track_read = false)
      : SpecificMemoryTracker([this](void* v, PageProtections p) { return DoHandleSegfault(v, p); }),
        page_size_(GetPageSize()),
        lock_(),
        ranges_(),
        cpu_written_pages_(),
        cpu_read_pages_(),
#define CONSTRUCT_SIGNAL_SAFE(function) \
  function(this, &MemoryTrackerImpl::function##Impl, &lock_, SIGSEGV)
        CONSTRUCT_SIGNAL_SAFE(EnableMemoryTracker),
        CONSTRUCT_SIGNAL_SAFE(TrackMappedMemory),
        CONSTRUCT_SIGNAL_SAFE(UntrackMappedMemory),
        CONSTRUCT_SIGNAL_SAFE(ForeachWrittenCPUPage),
        CONSTRUCT_SIGNAL_SAFE(ResetCPUReadPages),
#undef CONSTRUCT_SIGNAL_SAFE
#define CONSTRUCT_LOCKED(function) \
  function(this, &MemoryTrackerImpl::function##Impl, &lock_)
        CONSTRUCT_LOCKED(HandleSegfault)
#undef CONSTRUCT_LOCKED
  {
  }
  // Not copyable, not movable.
  MemoryTrackerImpl(const MemoryTrackerImpl&) = delete;
  MemoryTrackerImpl(MemoryTrackerImpl&&) = delete;
  MemoryTrackerImpl& operator=(const MemoryTrackerImpl&) = delete;
  MemoryTrackerImpl& operator=(MemoryTrackerImpl&&) = delete;

  ~MemoryTrackerImpl() {
    UntrackAllMappedMemory();
  }

 protected:

  // TrackMappedMemory registers the address of a GPU
  // region of memory in which to track writes.
  // Returns the pointer that should be used by the 
  // application.
  void TrackMappedMemoryImpl(void** memory, size_t size);

  // UntrackMappedMemory stops tracking the GPU memory
  // associated with this address.
  bool UntrackMappedMemoryImpl(void* memory, size_t size);

  // UntrackMappedMemory stops tracking the GPU memory
  // associated with this address.
  void UntrackAllMappedMemory();

  // GetAndResetCPUWrittenPages returns all pages written
  // on the CPU since the last time this was called.
  void ForeachWrittenCPUPageImpl(
    std::function<void(uintptr_t, uintptr_t)>);

  void ResetCPUReadPagesImpl();

  bool HandleSegfaultImpl(void* v, PageProtections p);

  // Dummy function that we can pass down to the specific memory tracker.
  bool DoHandleSegfault(void* v, PageProtections p) { return HandleSegfault(v, p); }

  const size_t page_size_;  // Size of a memory page in byte
  SpinLock lock_;              // Spin lock to guard the accesses of shared data
  struct range {
    size_t size;
    size_t allocatedSize;
    uintptr_t cpu_address;
  };

  struct allocation_info {
    void* gpu_addr;
    void* read_write_cpu_addr;
    void* cache_addr;
  };

  std::map<uintptr_t, range> ranges_;  // Memory ranges registered for tracking
  std::map<uintptr_t, allocation_info> cpu_alloc_info;  // Map of cpu range to gpu ranges

  DirtyPageTable cpu_written_pages_;    // All cpu_written pages
  DirtyPageTable cpu_read_pages_;    // All cpu_read pages

  struct page_status {
      PageProtections prot;
  };

  std::unordered_map<uintptr_t, page_status> status_;

  // A helper function to tell whether a given address is covered in a bunch of
  // memory ranges. If |page_aligned_ranges| is set to true, the ranges' address
  // will be aligned to page boundary, so if the |addr| is not in a range, but
  // shares a common memory page with the range, it will be considered as in the
  // range. By default |page_aligned_ranges| is set to false. Returns true if
  // the address is considered in the range, otherwise returns false.
  inline bool IsInRanges(uintptr_t addr, std::map<uintptr_t, range>& ranges,
                        bool page_aligned_ranges = false);

 public:
  size_t page_size() const { return page_size_; }

// SignalSafe wrapped methods that access shared data and cannot be
// interrupted by SIGSEGV signal.
#define SIGNAL_SAFE(function)                                                 \
  SignalSafe<MemoryTrackerImpl, decltype(&MemoryTrackerImpl::function##Impl)> \
      function;
  SIGNAL_SAFE(EnableMemoryTracker);
  SIGNAL_SAFE(TrackMappedMemory);
  SIGNAL_SAFE(UntrackMappedMemory);
  SIGNAL_SAFE(ForeachWrittenCPUPage);
  SIGNAL_SAFE(ResetCPUReadPages);
#undef SIGNAL_SAFE

// SpinLockGuarded wrapped methods that access critical region.
#define LOCKED(function)                                        \
  SpinLockGuarded<MemoryTrackerImpl,                            \
                  decltype(&MemoryTrackerImpl::function##Impl)> \
      function;
  LOCKED(HandleSegfault);
#undef LOCKED
};



template <typename SpecificMemoryTracker>
inline bool MemoryTrackerImpl<SpecificMemoryTracker>::IsInRanges(uintptr_t addr, std::map<uintptr_t, MemoryTrackerImpl<SpecificMemoryTracker>::range>& ranges,
                       bool page_aligned_ranges) {
  auto get_aligned_addr = [](uintptr_t addr) {
    return reinterpret_cast<uintptr_t>(
        GetAlignedAddress(reinterpret_cast<void*>(addr), GetPageSize()));
  };
  auto get_aligned_size = [](uintptr_t addr, size_t size) {
    return reinterpret_cast<uintptr_t>(
        GetAlignedSize(reinterpret_cast<void*>(addr), size, GetPageSize()));
  };
  // It is not safe to call std::prev() if the container is empty, so the empty
  // case is handled first.
  if (ranges.size() == 0) {
    return false;
  }
  auto it = ranges.lower_bound(addr);
  // Check if the lower bound range already covers the addr.
  if (it != ranges.end()) {
    if (it->first == addr) {
      return true;
    }
    if (page_aligned_ranges) {
      uintptr_t aligned_range_start = get_aligned_addr(it->first);
      if (aligned_range_start <= addr) {
        return true;
      }
    }
  }
  if (it == ranges.begin()) {
    return false;
  }
  // Check the previous range
  auto pit = std::prev(it, 1);
  uintptr_t range_start =
      page_aligned_ranges ? get_aligned_addr(pit->first) : pit->first;
  uintptr_t range_size = page_aligned_ranges
                             ? get_aligned_size(pit->first, pit->second.size)
                             : pit->second.size;
  if (addr < range_start || addr >= range_start + range_size) {
    return false;
  }
  return true;
}


}  // namespace track_memory
}  // namespace gapii

#if IS_POSIX
#include "core/memory_tracker/cc/posix/memory_tracker.inc"
#else
#include "core/memory_tracker/cc/windows/memory_tracker.inc"
#endif

#endif  // COHERENT_TRACKING_ENABLED
#endif  // GAPII_MEMORY_TRACKER_H
