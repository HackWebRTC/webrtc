/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/experiments/struct_parameters_parser.h"

#include <algorithm>

#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace struct_parser_impl {
namespace {
size_t FindOrEnd(absl::string_view str, size_t start, char delimiter) {
  size_t pos = str.find(delimiter, start);
  pos = (pos == std::string::npos) ? str.length() : pos;
  return pos;
}
}  // namespace

void ParseConfigParams(
    absl::string_view config_str,
    std::map<std::string, std::function<bool(absl::string_view)>> field_map) {
  size_t i = 0;
  while (i < config_str.length()) {
    size_t val_end = FindOrEnd(config_str, i, ',');
    size_t colon_pos = FindOrEnd(config_str, i, ':');
    size_t key_end = std::min(val_end, colon_pos);
    size_t val_begin = key_end + 1u;
    std::string key(config_str.substr(i, key_end - i));
    absl::string_view opt_value;
    if (val_end >= val_begin)
      opt_value = config_str.substr(val_begin, val_end - val_begin);
    i = val_end + 1u;
    auto field = field_map.find(key);
    if (field != field_map.end()) {
      if (!field->second(opt_value)) {
        RTC_LOG(LS_WARNING) << "Failed to read field with key: '" << key
                            << "' in trial: \"" << config_str << "\"";
      }
    } else {
      RTC_LOG(LS_INFO) << "No field with key: '" << key
                       << "' (found in trial: \"" << config_str << "\")";
    }
  }
}

std::string EncodeStringStringMap(std::map<std::string, std::string> mapping) {
  rtc::StringBuilder sb;
  bool first = true;
  for (const auto& kv : mapping) {
    if (!first)
      sb << ",";
    sb << kv.first << ":" << kv.second;
    first = false;
  }
  return sb.Release();
}
}  // namespace struct_parser_impl

}  // namespace webrtc
