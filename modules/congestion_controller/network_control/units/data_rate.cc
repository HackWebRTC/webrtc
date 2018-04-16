/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/network_control/units/data_rate.h"

#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace {
int64_t Microbytes(const DataSize& size) {
  constexpr int64_t kMaxBeforeConversion =
      std::numeric_limits<int64_t>::max() / 1000000;
  RTC_DCHECK(size.bytes() < kMaxBeforeConversion)
      << "size is too large to be expressed in microbytes, size: "
      << size.bytes() << " is not less than " << kMaxBeforeConversion;
  return size.bytes() * 1000000;
}
}  // namespace

DataRate operator/(const DataSize& size, const TimeDelta& duration) {
  return DataRate::bytes_per_second(Microbytes(size) / duration.us());
}

TimeDelta operator/(const DataSize& size, const DataRate& rate) {
  return TimeDelta::us(Microbytes(size) / rate.bytes_per_second());
}

DataSize operator*(const DataRate& rate, const TimeDelta& duration) {
  int64_t microbytes = rate.bytes_per_second() * duration.us();
  return DataSize::bytes((microbytes + 500000) / 1000000);
}

DataSize operator*(const TimeDelta& duration, const DataRate& rate) {
  return rate * duration;
}

std::string ToString(const DataRate& value) {
  char buf[64];
  rtc::SimpleStringBuilder sb(buf);
  if (value.IsInfinite()) {
    sb << "inf bps";
  } else if (!value.IsInitialized()) {
    sb << "? bps";
  } else {
    sb << value.bps() << " bps";
  }
  return sb.str();
}
}  // namespace webrtc
