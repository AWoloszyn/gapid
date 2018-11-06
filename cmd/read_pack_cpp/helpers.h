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
#include "cmd/read_pack_cpp/vulkan_replay_types.h"

template<typename T>
void do_remap(T& t) {

}

template<typename T>
void write(gapil::Slice<T>, uint64_t, T&) {}

template<typename T>
gapil::Slice<T> make(uint64_t _x) {
    // Make a new pool here.
    return gapil::Slice<T>(nullptr, _x);
}

template<typename T>
gapil::Slice<T>& clone(const gapil::Slice<T> src) {
 auto dst = make<T>(src.count());
  // Make sure that we actually fill the data the first time.
  // If we use ::copy(), then the copy will only happen if
  // the observer is active.
  remap(src);
  src.copy(dst, 0, src.count(), 0);
  return dst;
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
T* remap_pointer(gapil::Slice<T*> _x, size_t count) {
    fixup_pointer(_x[count]);
    return _x[count];
}

void* fixup_pointer(void** _v) {return *_v;}


#endif // READ_PACK_CPP_HELPERS_H__