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

#include "cmd/read_pack_cpp/helpers3.h"
#include "gapis/memory/memory_pb/memory.pb.h"
#include "gapis/capture/capture.pb.h"
#include <fstream>

inline int64_t read_zz(std::ifstream* _stream)
{
    uint64_t n = 0;
    int shift = 0;
    char val = _stream->get();
    while (val & 0x80)
    {
        n |= static_cast<uint64_t>(val & 0x7F) << shift;
        shift += 7;
        val = _stream->get();
    }
    n |= val << shift;
    return n;
}

inline int32_t read_zz32(std::ifstream* _stream) {
  int64_t x = read_zz(_stream);
  return int32_t(uint64_t((uint32_t(x) >> 1) ^ uint32_t((int32_t(x&1)<<31)>>31)));
}

bool get_next_write_to(void* pos, std::vector<uint8_t>& _fill) {

  // This is the mapped pointer, so we have to work backwards
  auto it = _remapped_ranges_rev.upper_bound(pos);
  if (it == _remapped_ranges_rev.end()) {
    it = _remapped_ranges_rev.begin();
    // Ugly hack for now.
    for (size_t i = 0; i < _remapped_ranges_rev.size() - 1; ++i) {
      it++;
    }
  }
  else {
    --it;
  }

  uintptr_t offset = (char*)pos - (char*)it->first;
  uintptr_t base = it->second + offset;

  
  auto old_pos = _stream->tellg();
  while(_stream->good()) {
    int64_t sz = read_zz32(_stream);
    if (sz >= 0) {
        int64_t offs = _stream->tellg();
        int32_t parent = read_zz32(_stream);
        (void)parent;
        int64_t new_offs = _stream->tellg();
        if (new_offs - offs == sz) {
          continue;
        }
        int32_t type = read_zz32(_stream);
        if (type < 0) { type = -type; }

        offs = (_stream->tellg() - offs);
        std::string t = _types[type];
        if (t == "memory.Observation") {
            std::string d;
            d.resize(sz - offs);
            _stream->read(&d[0], sz - offs);

            memory::Observation obs;
            obs.ParseFromString(d);
            if (obs.base() < base && obs.base() + obs.size() > base + _fill.size()) {
                // Get our resource
                auto new_pos = _resources[obs.res_index()];
                _stream->seekg(new_pos.first);
                std::string s;
                s.resize(new_pos.second);
                _stream->read(&s[0], new_pos.second);
                _stream->seekg(old_pos);

                capture::Resource res;
                auto a = res.ParseFromString(s);
                //This is our observation
                uint64_t base_offs = base - obs.base();
                memcpy(_fill.data(), res.data().c_str() + base_offs, _fill.size());
                return true;
            }
        } else {
            _stream->ignore(sz - offs);
        }
    } else {
        _stream->ignore(-sz);
    }
  }
  _reset_stream();
  _stream->seekg(old_pos);
  return false;
}

std::string get_next_object(std::string _type) {
  auto old_pos = _stream->tellg();
  while (_stream->good()) {
    int64_t sz = read_zz32(_stream);
    if (sz >= 0) {
      int64_t offs = _stream->tellg();
      int32_t parent = read_zz32(_stream);
      (void)parent;
      int64_t new_offs = _stream->tellg();
      if (new_offs - offs == sz) {
        return "";
      }
      int32_t type = read_zz32(_stream);
      if (type < 0) { type = -type; }

      offs = (_stream->tellg() - offs);
      std::string t = _types[type];
      if (t == _type) {
        std::string d;
        d.resize(sz - offs);
        _stream->read(&d[0], sz - offs);
        _stream->seekg(old_pos);
        return std::move(d);
      } else {
        _stream->ignore(sz - offs);
      }
    }
    else {
      _stream->ignore(-sz);
    }
  }
  _reset_stream();
  _stream->seekg(old_pos);
  return "";
}
