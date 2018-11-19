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

#ifndef READ_PACK_CPP_HELPERS2_H__
#define READ_PACK_CPP_HELPERS2_H__

#include "cmd/read_pack_cpp/vulkan_replay_imports.h"
#include <map>
#include <list>
#include <functional>

extern gapii::VulkanImports mImports;
extern gapii::VulkanState mState;
extern std::ifstream* _stream;
extern std::function<void()> _reset_stream;
extern std::unordered_map<int64_t, std::string> _types;
extern std::unordered_map<int64_t, std::pair<int64_t, int64_t>> _resources;
extern core::IntervalList<uintptr_t> _all_ranges;


struct pending_write {
  uintptr_t start;
  uintptr_t size;
  std::function<void()> func;
};
extern std::list<pending_write> _pending_writes;


#endif // READ_PACK_CPP_HELPERS2_H__
