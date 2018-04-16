/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_UNITS_TIME_DELTA_H_
#define MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_UNITS_TIME_DELTA_H_

#include <stdint.h>
#include <cmath>
#include <limits>
#include <string>

#include "rtc_base/checks.h"

namespace webrtc {
namespace timedelta_impl {
constexpr int64_t kPlusInfinityVal = std::numeric_limits<int64_t>::max();
constexpr int64_t kMinusInfinityVal = std::numeric_limits<int64_t>::min();
constexpr int64_t kSignedNotInitializedVal = kMinusInfinityVal + 1;

}  // namespace timedelta_impl

// TimeDelta represents the difference between two timestamps. Commonly this can
// be a duration. However since two Timestamps are not guaranteed to have the
// same epoch (they might come from different computers, making exact
// synchronisation infeasible), the duration covered by a TimeDelta can be
// undefined. To simplify usage, it can be constructed and converted to
// different units, specifically seconds (s), milliseconds (ms) and
// microseconds (us).
class TimeDelta {
 public:
  TimeDelta() : TimeDelta(timedelta_impl::kSignedNotInitializedVal) {}
  static TimeDelta Zero() { return TimeDelta(0); }
  static TimeDelta PlusInfinity() {
    return TimeDelta(timedelta_impl::kPlusInfinityVal);
  }
  static TimeDelta MinusInfinity() {
    return TimeDelta(timedelta_impl::kMinusInfinityVal);
  }
  static TimeDelta seconds(int64_t seconds) {
    return TimeDelta::us(seconds * 1000000);
  }
  static TimeDelta ms(int64_t milliseconds) {
    return TimeDelta::us(milliseconds * 1000);
  }
  static TimeDelta us(int64_t microseconds) {
    // Infinities only allowed via use of explicit constants.
    RTC_DCHECK(microseconds > std::numeric_limits<int64_t>::min());
    RTC_DCHECK(microseconds < std::numeric_limits<int64_t>::max());
    RTC_DCHECK(microseconds != timedelta_impl::kSignedNotInitializedVal);
    return TimeDelta(microseconds);
  }
  int64_t seconds() const {
    return (us() + (us() >= 0 ? 500000 : -500000)) / 1000000;
  }
  int64_t ms() const { return (us() + (us() >= 0 ? 500 : -500)) / 1000; }
  int64_t us() const {
    RTC_DCHECK(IsFinite());
    return microseconds_;
  }

  double SecondsAsDouble() const;

  TimeDelta Abs() const { return TimeDelta::us(std::abs(us())); }
  bool IsZero() const { return microseconds_ == 0; }
  bool IsFinite() const { return IsInitialized() && !IsInfinite(); }
  bool IsInitialized() const {
    return microseconds_ != timedelta_impl::kSignedNotInitializedVal;
  }
  bool IsInfinite() const {
    return microseconds_ == timedelta_impl::kPlusInfinityVal ||
           microseconds_ == timedelta_impl::kMinusInfinityVal;
  }
  bool IsPlusInfinity() const {
    return microseconds_ == timedelta_impl::kPlusInfinityVal;
  }
  bool IsMinusInfinity() const {
    return microseconds_ == timedelta_impl::kMinusInfinityVal;
  }
  TimeDelta operator+(const TimeDelta& other) const {
    return TimeDelta::us(us() + other.us());
  }
  TimeDelta operator-(const TimeDelta& other) const {
    return TimeDelta::us(us() - other.us());
  }
  TimeDelta& operator-=(const TimeDelta& other) {
    microseconds_ -= other.us();
    return *this;
  }
  TimeDelta& operator+=(const TimeDelta& other) {
    microseconds_ += other.us();
    return *this;
  }
  TimeDelta operator*(double scalar) const {
    return TimeDelta::us(std::round(us() * scalar));
  }
  TimeDelta operator*(int64_t scalar) const {
    return TimeDelta::us(us() * scalar);
  }
  TimeDelta operator*(int32_t scalar) const {
    return TimeDelta::us(us() * scalar);
  }
  TimeDelta operator/(int64_t scalar) const {
    return TimeDelta::us(us() / scalar);
  }
  bool operator==(const TimeDelta& other) const {
    return microseconds_ == other.microseconds_;
  }
  bool operator!=(const TimeDelta& other) const {
    return microseconds_ != other.microseconds_;
  }
  bool operator<=(const TimeDelta& other) const {
    return microseconds_ <= other.microseconds_;
  }
  bool operator>=(const TimeDelta& other) const {
    return microseconds_ >= other.microseconds_;
  }
  bool operator>(const TimeDelta& other) const {
    return microseconds_ > other.microseconds_;
  }
  bool operator<(const TimeDelta& other) const {
    return microseconds_ < other.microseconds_;
  }

 private:
  explicit TimeDelta(int64_t us) : microseconds_(us) {}
  int64_t microseconds_;
};
inline TimeDelta operator*(const double& scalar, const TimeDelta& delta) {
  return delta * scalar;
}
inline TimeDelta operator*(const int64_t& scalar, const TimeDelta& delta) {
  return delta * scalar;
}
inline TimeDelta operator*(const int32_t& scalar, const TimeDelta& delta) {
  return delta * scalar;
}

std::string ToString(const TimeDelta& value);
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_UNITS_TIME_DELTA_H_
