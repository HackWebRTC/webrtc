/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/unique_id_generator.h"

#include <limits>
#include <vector>

#include "rtc_base/helpers.h"
#include "rtc_base/string_to_number.h"
#include "rtc_base/stringencode.h"

namespace webrtc {

namespace {
UniqueNumberGenerator<uint32_t> CreateUniqueNumberGenerator(
    rtc::ArrayView<std::string> known_ids) {
  std::vector<uint32_t> known_ints;
  for (const std::string& str : known_ids) {
    absl::optional<uint32_t> value = rtc::StringToNumber<uint32_t>(str);
    if (value.has_value()) {
      known_ints.push_back(value.value());
    }
  }
  return UniqueNumberGenerator<uint32_t>(known_ints);
}
}  // namespace

UniqueRandomIdGenerator::UniqueRandomIdGenerator() : known_ids_() {}
UniqueRandomIdGenerator::UniqueRandomIdGenerator(
    rtc::ArrayView<uint32_t> known_ids)
    : known_ids_(known_ids.begin(), known_ids.end()) {}

UniqueRandomIdGenerator::~UniqueRandomIdGenerator() = default;

uint32_t UniqueRandomIdGenerator::GenerateId() {
  while (true) {
    RTC_CHECK_LT(known_ids_.size(), std::numeric_limits<uint32_t>::max());
    auto pair = known_ids_.insert(rtc::CreateRandomNonZeroId());
    if (pair.second) {
      return *pair.first;
    }
  }
}

UniqueStringGenerator::UniqueStringGenerator() : unique_number_generator_() {}
UniqueStringGenerator::UniqueStringGenerator(
    rtc::ArrayView<std::string> known_ids)
    : unique_number_generator_(CreateUniqueNumberGenerator(known_ids)) {}

UniqueStringGenerator::~UniqueStringGenerator() = default;

std::string UniqueStringGenerator::GenerateString() {
  return rtc::ToString(unique_number_generator_.GenerateNumber());
}

}  // namespace webrtc
