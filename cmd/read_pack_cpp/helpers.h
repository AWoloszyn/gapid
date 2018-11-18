/*
 * Copyright (C) 2018 Google Inc.
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

#ifndef READ_PACK_CPP_HELPERS_H__
#define READ_PACK_CPP_HELPERS_H__

#include "gapil/runtime/cc/runtime.h"
#include "gapil/runtime/cc/slice.inc"
#include "gapil/runtime/cc/string.h"
#include "core/cc/interval_list.h"

#include <map>

extern std::map<uintptr_t, std::pair<void*, uintptr_t>> _remapped_ranges;
extern std::map<void*, uintptr_t> _remapped_ranges_rev;

template<typename T>
T do_remap(T& t) {
  return t;
}

template<typename T>
void write(gapil::Slice<T>, uint64_t) {}

template<typename T>
gapil::Slice<T> make(uint64_t _x) {
    // Make a new pool here.
    return gapil::Slice<T>(nullptr, _x);
}

std::string inline make_string(const char* str) {
  if (str == nullptr) {
    return std::string();
  }
  return std::string(str);
}

template<typename T>
gapil::Slice<T> make_slice(T* _t, size_t offset, size_t count) {
    return gapil::Slice<T>(_t + offset, count);
}

template<typename T>
gapil::Slice<T> get_slice(T* _t) {
    return gapil::Slice<T>(_t, 1);
}

template<typename T>
T* fixup_pointer(T** _v, std::map<uintptr_t, std::pair<void*, uintptr_t>>* _map) {
  if (*_v == nullptr) return nullptr;
  void* ptr = *_v;

  auto it = _map->upper_bound((uintptr_t)*_v);
  if (it == _map->begin()) {
    return nullptr;
  }
  if (it == _map->end()) {
    auto& r = _map->rbegin();
    auto diff = (char*)ptr - (char*)r->first;
    if (diff > r->second.second) {
      return nullptr;
    }

    *_v = (T*)(((char*)r->second.first) + diff);
    return *_v;
  }
  --it;

  auto diff = (char*)ptr - (char*)it->first;
  if (diff > it->second.second) {
    return nullptr;
  }
  *_v = (T*)(((char*)it->second.first) + diff);
  return *_v;
}

template<typename T>
T* fixup_pointer(T** _v) {
  if (fixup_pointer(_v, &_mapped_ranges)) {
    return *_v;
  }
  if (fixup_pointer(_v, &_remapped_ranges)) {
    return *_v;
  }
  return *_v;
}

template<typename T>
T* remap_pointer(gapil::Slice<T*> _x, size_t count) {
    fixup_pointer((void**)&_x[count]);
    return _x[count];
}

template<typename T>
gapil::Slice<T*> remap_pointer(gapil::Slice<T*> _x) {
    for (size_t i = 0; i < _x.count(); ++i) {
        fixup_pointer((void**)&_x[i]);
    }
    return _x;
}

template<typename T>
gapil::Slice<T> clone(gapil::Slice<T> src) {
    auto dst = make<T>(src.count());
    // Make sure that we actually fill the data the first time.
    // If we use ::copy(), then the copy will only happen if
    // the observer is active.
    remap(src);
    src.copy(dst, 0, src.count(), 0);
    return dst;
}


template<typename T>
gapil::Slice<T> clone_builtin(gapil::Slice<T> src) {
    auto dst = make<T>(src.count());
    //src.copy(dst, 0, src.count(), 0);
    return dst;
}

#endif // READ_PACK_CPP_HELPERS_H__