/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_UNITS_DATA_RATE_H_
#define MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_UNITS_DATA_RATE_H_

#include <stdint.h>
#include <cmath>
#include <limits>
#include <string>

#include "rtc_base/checks.h"

#include "modules/congestion_controller/network_control/units/data_size.h"
#include "modules/congestion_controller/network_control/units/time_delta.h"

namespace webrtc {
namespace data_rate_impl {
constexpr int64_t kPlusInfinityVal = std::numeric_limits<int64_t>::max();
constexpr int64_t kNotInitializedVal = -1;
}  // namespace data_rate_impl

// DataRate is a class that represents a given data rate. This can be used to
// represent bandwidth, encoding bitrate, etc. The internal storage is currently
// bits per second (bps) since this makes it easier to intepret the raw value
// when debugging. The promised precision, however is only that it will
// represent bytes per second accurately. Any implementation depending on bps
// resolution should document this by changing this comment.
class DataRate {
 public:
  DataRate() : DataRate(data_rate_impl::kNotInitializedVal) {}
  static DataRate Zero() { return DataRate(0); }
  static DataRate Infinity() {
    return DataRate(data_rate_impl::kPlusInfinityVal);
  }
  static DataRate bytes_per_second(int64_t bytes_per_sec) {
    RTC_DCHECK_GE(bytes_per_sec, 0);
    return DataRate(bytes_per_sec * 8);
  }
  static DataRate bits_per_second(int64_t bits_per_sec) {
    RTC_DCHECK_GE(bits_per_sec, 0);
    return DataRate(bits_per_sec);
  }
  static DataRate bps(int64_t bits_per_sec) {
    return DataRate::bits_per_second(bits_per_sec);
  }
  static DataRate kbps(int64_t kilobits_per_sec) {
    return DataRate::bits_per_second(kilobits_per_sec * 1000);
  }
  int64_t bits_per_second() const {
    RTC_DCHECK(IsFinite());
    return bits_per_sec_;
  }
  int64_t bytes_per_second() const { return bits_per_second() / 8; }
  int64_t bps() const { return bits_per_second(); }
  int64_t bps_or(int64_t fallback) const {
    return IsFinite() ? bits_per_second() : fallback;
  }
  int64_t kbps() const { return (bps() + 500) / 1000; }
  bool IsZero() const { return bits_per_sec_ == 0; }
  bool IsInfinite() const {
    return bits_per_sec_ == data_rate_impl::kPlusInfinityVal;
  }
  bool IsInitialized() const {
    return bits_per_sec_ != data_rate_impl::kNotInitializedVal;
  }
  bool IsFinite() const { return IsInitialized() && !IsInfinite(); }
  DataRate operator*(double scalar) const {
    return DataRate::bytes_per_second(std::round(bytes_per_second() * scalar));
  }
  DataRate operator*(int64_t scalar) const {
    return DataRate::bytes_per_second(bytes_per_second() * scalar);
  }
  DataRate operator*(int32_t scalar) const {
    return DataRate::bytes_per_second(bytes_per_second() * scalar);
  }
  bool operator==(const DataRate& other) const {
    return bits_per_sec_ == other.bits_per_sec_;
  }
  bool operator!=(const DataRate& other) const {
    return bits_per_sec_ != other.bits_per_sec_;
  }
  bool operator<=(const DataRate& other) const {
    return bits_per_sec_ <= other.bits_per_sec_;
  }
  bool operator>=(const DataRate& other) const {
    return bits_per_sec_ >= other.bits_per_sec_;
  }
  bool operator>(const DataRate& other) const {
    return bits_per_sec_ > other.bits_per_sec_;
  }
  bool operator<(const DataRate& other) const {
    return bits_per_sec_ < other.bits_per_sec_;
  }

 private:
  // Bits per second used internally to simplify debugging by making the value
  // more recognizable.
  explicit DataRate(int64_t bits_per_second) : bits_per_sec_(bits_per_second) {}
  int64_t bits_per_sec_;
};
inline DataRate operator*(const double& scalar, const DataRate& rate) {
  return rate * scalar;
}
inline DataRate operator*(const int64_t& scalar, const DataRate& rate) {
  return rate * scalar;
}
inline DataRate operator*(const int32_t& scalar, const DataRate& rate) {
  return rate * scalar;
}

DataRate operator/(const DataSize& size, const TimeDelta& duration);
TimeDelta operator/(const DataSize& size, const DataRate& rate);
DataSize operator*(const DataRate& rate, const TimeDelta& duration);
DataSize operator*(const TimeDelta& duration, const DataRate& rate);

std::string ToString(const DataRate& value);

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_UNITS_DATA_RATE_H_
