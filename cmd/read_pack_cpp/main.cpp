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

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

#include "cmd/read_pack_cpp/vulkan_replay_types.h"
#include "gapis/memory/memory_pb/memory.pb.h"
#include "gapis/capture/capture.pb.h"
#include "core/cc/interval_list.h"

#include <unordered_map>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/descriptor.pb.h>

#if _WIN32
#include <fcntl.h>
#include <stdio.h>
#endif

#define ASSERT(x) _assert(x, __FILE__, __LINE__, #x)

void _assert(bool b, const char* file, size_t line, const char* msg) {
  if (!b) {
    std::cerr << "Assert failed: " << file << ":" << line << ": " << msg << std::endl;
    throw std::exception();
  }
}

int64_t read_zigzag(std::ifstream* _stream)
{
    uint64_t n = 0;
    int shift = 0;
    char val = _stream->get();
    ASSERT(!_stream->fail());
    while (val & 0x80)
    {
        n |= static_cast<uint64_t>(val & 0x7F) << shift;
        shift += 7;
        val = _stream->get();
        ASSERT(!_stream->fail());
    }
    n |= val << shift;
    return n;
}

int32_t read_zigzag32(std::ifstream* _stream) {
  int64_t x = read_zigzag(_stream);
  return int32_t(uint64_t((uint32_t(x) >> 1) ^ uint32_t((int32_t(x&1)<<31)>>31)));
}

void read_header(std::ifstream* _stream) {
  char header[16];
  _stream->read(header, 16);
  ASSERT(!_stream->fail());
  ASSERT(strncmp(header, "ProtoPack\r\n", 11) == 0);
  // This is definitely a protopack file.
}

std::unordered_map<int64_t, std::string> _types;
std::unordered_map<int64_t, int64_t> _resources;
core::IntervalList<uintptr_t> _all_ranges;
std::unordered_map<uintptr_t, void*> _remapped_ranges;

bool read_object(int64_t size, std::ifstream* _stream) {
  int64_t offs = _stream->tellg();
  if(_stream->fail()) {
    return false;
  }
  int32_t parent = read_zigzag32(_stream);
  (void)parent;
  int64_t new_offs = _stream->tellg();
  if (new_offs - offs == size) {
      return true;
  }
  int32_t type = read_zigzag32(_stream);
  if (type < 0) { type = -type; }

  offs = (_stream->tellg() - offs);

  if (_types[type] == "memory.Observation") {
    std::string d;
    d.resize(size - offs);
    _stream->read(&d[0], size - offs);

    memory::Observation obs;
    obs.ParseFromString(d);
    if (obs.pool() == 0) {
      _all_ranges.merge(core::Interval<uintptr_t>({obs.base(), obs.base() + obs.size()}));
    }
    if (obs.base() == 0 && obs.pool() == 0) {
      std::string output;
      google::protobuf::util::JsonPrintOptions options;
      options.add_whitespace = true;
      if (google::protobuf::util::Status::OK !=
            google::protobuf::util::MessageToJsonString(obs, &output,
                                                        options)) {
      }
      std::cout << "" << output << std::endl;
    }
  } else if (_types[type] == "capture.Resource") {
    _resources[_resources.size() + 1] = _stream->tellg();
    std::string d;
    d.resize(size - offs);
    _stream->read(&d[0], size - offs);

    capture::Resource res;
    res.ParseFromString(d);
  } else {
    _stream->ignore(size - offs);
  }

  return true;
}

bool read_type(int64_t size, std::ifstream* _stream) {
  size = -size;
  // Read a proto string here
  // ignore the descriptor
  int64_t offs = _stream->tellg();

  int64_t len = read_zigzag(_stream);
  std::string my_string;
  my_string.resize(len);
  _stream->read(&my_string[0], len);
  if (_stream->fail()) {
    return false;
  }

  google::protobuf::DescriptorProto descriptor;

  offs = ( _stream->tellg() - offs);
  std::string d;
  d.resize(size - offs);
  _stream->read(&d[0], size - offs);
  if (_stream->fail()) {
    return false;
  }

  _types[_types.size() + 1] = std::move(my_string);
  return true;
}

bool read_chunk(std::ifstream* _stream) {
  int64_t sz = read_zigzag32(_stream);
  if (sz >= 0) {
    // This is an object
    return read_object(sz, _stream);
  } else {
    // This is a type
    return read_type(sz, _stream);
  }
}

int main(int argc, char const *argv[]) {
  _all_ranges.setMergeThreshold(1024*1024*16);
  std::ifstream capture(argv[argc-1], std::ios::in | std::ios::binary);
  read_header(&capture);
  try {
    while(read_chunk(&capture)) {
    }
  } catch (const std::exception& e){
  }

  std::cout << "NResources: " << _resources.size() << std::endl;
  std::cout << "NTypes: " << _types.size() << std::endl;
  std::cout << "NRanges: " << _all_ranges.count() << std::endl;

  uintptr_t rangeSize = 0;
  for (auto& x: _all_ranges) {
    rangeSize += x.mEnd - x.mStart;
    std::cout << "Range: " << x.mStart << ":" << x.mEnd << std::endl;
    _remapped_ranges[x.mStart] = malloc(x.mEnd - x.mStart);
  }

  std::cout << "Total range size: " << rangeSize << std::endl;

  return 0;
}
