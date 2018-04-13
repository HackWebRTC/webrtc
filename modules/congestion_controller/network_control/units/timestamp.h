/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_UNITS_TIMESTAMP_H_
#define MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_UNITS_TIMESTAMP_H_

#include <stdint.h>
#include <limits>
#include <string>

#include "modules/congestion_controller/network_control/units/time_delta.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace timestamp_impl {
constexpr int64_t kPlusInfinityVal = std::numeric_limits<int64_t>::max();
constexpr int64_t kMinusInfinityVal = std::numeric_limits<int64_t>::min();
constexpr int64_t kSignedNotInitializedVal = kMinusInfinityVal + 1;
constexpr int64_t kNotInitializedVal = -1;
}  // namespace timestamp_impl

// Timestamp represents the time that has passed since some unspecified epoch.
// The epoch is assumed to be before any represented timestamps, this means that
// negative values are not valid. The most notable feature is that the
// difference of two Timestamps results in a TimeDelta.
class Timestamp {
 public:
  Timestamp() : Timestamp(timestamp_impl::kNotInitializedVal) {}
  static Timestamp Infinity() {
    return Timestamp(timestamp_impl::kPlusInfinityVal);
  }
  static Timestamp seconds(int64_t seconds) { return Timestamp::s(seconds); }
  static Timestamp s(int64_t seconds) {
    return Timestamp::us(seconds * 1000000);
  }
  static Timestamp ms(int64_t millis) { return Timestamp::us(millis * 1000); }
  static Timestamp us(int64_t micros) {
    RTC_DCHECK_GE(micros, 0);
    return Timestamp(micros);
  }
  int64_t s() const { return (us() + 500000) / 1000000; }
  int64_t ms() const { return (us() + 500) / 1000; }
  int64_t us() const {
    RTC_DCHECK(IsFinite());
    return microseconds_;
  }

  double SecondsAsDouble() const;

  bool IsInfinite() const {
    return microseconds_ == timestamp_impl::kPlusInfinityVal;
  }
  bool IsInitialized() const {
    return microseconds_ != timestamp_impl::kNotInitializedVal;
  }
  bool IsFinite() const { return IsInitialized() && !IsInfinite(); }
  TimeDelta operator-(const Timestamp& other) const {
    return TimeDelta::us(us() - other.us());
  }
  Timestamp operator-(const TimeDelta& delta) const {
    return Timestamp::us(us() - delta.us());
  }
  Timestamp operator+(const TimeDelta& delta) const {
    return Timestamp::us(us() + delta.us());
  }
  Timestamp& operator-=(const TimeDelta& other) {
    microseconds_ -= other.us();
    return *this;
  }
  Timestamp& operator+=(const TimeDelta& other) {
    microseconds_ += other.us();
    return *this;
  }
  bool operator==(const Timestamp& other) const {
    return microseconds_ == other.microseconds_;
  }
  bool operator!=(const Timestamp& other) const {
    return microseconds_ != other.microseconds_;
  }
  bool operator<=(const Timestamp& other) const { return us() <= other.us(); }
  bool operator>=(const Timestamp& other) const { return us() >= other.us(); }
  bool operator>(const Timestamp& other) const { return us() > other.us(); }
  bool operator<(const Timestamp& other) const { return us() < other.us(); }

 private:
  explicit Timestamp(int64_t us) : microseconds_(us) {}
  int64_t microseconds_;
};

std::string ToString(const Timestamp& value);

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_UNITS_TIMESTAMP_H_
