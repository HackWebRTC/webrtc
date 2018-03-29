/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/network_control/include/network_units.h"
#include <cmath>

namespace webrtc {
TimeDelta TimeDelta::operator*(double scalar) const {
  return TimeDelta::us(std::round(us() * scalar));
}

DataSize DataSize::operator*(double scalar) const {
  return DataSize::bytes(std::round(bytes() * scalar));
}

DataRate DataRate::operator*(double scalar) const {
  return DataRate::bytes_per_second(std::round(bytes_per_second() * scalar));
}

DataRate operator/(const DataSize& size, const TimeDelta& duration) {
  RTC_DCHECK(size.bytes() < std::numeric_limits<int64_t>::max() / 1000000)
      << "size is too large, size: " << size.bytes() << " is not less than "
      << std::numeric_limits<int64_t>::max() / 1000000;
  auto bytes_per_sec = size.bytes() * 1000000 / duration.us();
  return DataRate::bytes_per_second(bytes_per_sec);
}

TimeDelta operator/(const DataSize& size, const DataRate& rate) {
  RTC_DCHECK(size.bytes() < std::numeric_limits<int64_t>::max() / 1000000)
      << "size is too large, size: " << size.bytes() << " is not less than "
      << std::numeric_limits<int64_t>::max() / 1000000;
  auto microseconds = size.bytes() * 1000000 / rate.bytes_per_second();
  return TimeDelta::us(microseconds);
}

DataSize operator*(const DataRate& rate, const TimeDelta& duration) {
  auto micro_bytes = rate.bytes_per_second() * duration.us();
  auto bytes = units_internal::DivideAndRound(micro_bytes, 1000000);
  return DataSize::bytes(bytes);
}

DataSize operator*(const TimeDelta& duration, const DataRate& rate) {
  return rate * duration;
}

::std::ostream& operator<<(::std::ostream& os, const DataRate& value) {
  if (value.IsInfinite()) {
    return os << "inf bps";
  } else if (!value.IsInitialized()) {
    return os << "? bps";
  } else {
    return os << value.bps() << " bps";
  }
}
::std::ostream& operator<<(::std::ostream& os, const DataSize& value) {
  if (value.IsInfinite()) {
    return os << "inf bytes";
  } else if (!value.IsInitialized()) {
    return os << "? bytes";
  } else {
    return os << value.bytes() << " bytes";
  }
}
::std::ostream& operator<<(::std::ostream& os, const Timestamp& value) {
  if (value.IsInfinite()) {
    return os << "inf ms";
  } else if (!value.IsInitialized()) {
    return os << "? ms";
  } else {
    return os << value.ms() << " ms";
  }
}
::std::ostream& operator<<(::std::ostream& os, const TimeDelta& value) {
  if (value.IsPlusInfinity()) {
    return os << "+inf ms";
  } else if (value.IsMinusInfinity()) {
    return os << "-inf ms";
  } else if (!value.IsInitialized()) {
    return os << "? ms";
  } else {
    return os << value.ms() << " ms";
  }
}

}  // namespace webrtc
