/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_INCLUDE_NETWORK_UNITS_H_
#define MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_INCLUDE_NETWORK_UNITS_H_
#include <stdint.h>
#include <limits>
#include "rtc_base/checks.h"

namespace webrtc {
namespace units_internal {
constexpr int64_t kPlusInfinityVal = std::numeric_limits<int64_t>::max();
constexpr int64_t kMinusInfinityVal = std::numeric_limits<int64_t>::min();
constexpr int64_t kSignedNotInitializedVal = kMinusInfinityVal + 1;
constexpr int64_t kNotInitializedVal = -1;

inline int64_t DivideAndRound(int64_t numerator, int64_t denominators) {
  if (numerator >= 0) {
    return (numerator + (denominators / 2)) / denominators;
  } else {
    return (numerator + (denominators / 2)) / denominators - 1;
  }
}
}  // namespace units_internal

// TimeDelta represents the difference between two timestamps. Commonly this can
// be a duration. However since two Timestamps are not guaranteed to have the
// same epoch (they might come from different computers, making exact
// synchronisation infeasible), the duration covered by a TimeDelta can be
// undefined. To simplify usage, it can be constructed and converted to
// different units, specifically seconds (s), milliseconds (ms) and
// microseconds (us).
class TimeDelta {
 public:
  TimeDelta() : TimeDelta(units_internal::kSignedNotInitializedVal) {}
  static TimeDelta Zero() { return TimeDelta(0); }
  static TimeDelta PlusInfinity() {
    return TimeDelta(units_internal::kPlusInfinityVal);
  }
  static TimeDelta MinusInfinity() {
    return TimeDelta(units_internal::kMinusInfinityVal);
  }
  static TimeDelta seconds(int64_t seconds) { return TimeDelta::s(seconds); }
  static TimeDelta s(int64_t seconds) {
    return TimeDelta::us(seconds * 1000000);
  }
  static TimeDelta ms(int64_t milliseconds) {
    return TimeDelta::us(milliseconds * 1000);
  }
  static TimeDelta us(int64_t microseconds) {
    // Infinities only allowed via use of explicit constants.
    RTC_DCHECK(microseconds > std::numeric_limits<int64_t>::min());
    RTC_DCHECK(microseconds < std::numeric_limits<int64_t>::max());
    return TimeDelta(microseconds);
  }
  int64_t s() const { return units_internal::DivideAndRound(us(), 1000000); }
  int64_t ms() const { return units_internal::DivideAndRound(us(), 1000); }
  int64_t us() const {
    RTC_DCHECK(IsFinite());
    return microseconds_;
  }
  TimeDelta Abs() const { return TimeDelta::us(std::abs(us())); }

  double SecondsAsDouble() const;

  bool IsZero() const { return microseconds_ == 0; }
  bool IsFinite() const { return IsInitialized() && !IsInfinite(); }
  bool IsInitialized() const {
    return microseconds_ != units_internal::kSignedNotInitializedVal;
  }
  bool IsInfinite() const {
    return microseconds_ == units_internal::kPlusInfinityVal ||
           microseconds_ == units_internal::kMinusInfinityVal;
  }
  bool IsPlusInfinity() const {
    return microseconds_ == units_internal::kPlusInfinityVal;
  }
  bool IsMinusInfinity() const {
    return microseconds_ == units_internal::kMinusInfinityVal;
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
  TimeDelta operator*(double scalar) const;
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

// Timestamp represents the time that has passed since some unspecified epoch.
// The epoch is assumed to be before any represented timestamps, this means that
// negative values are not valid. The most notable feature is that the
// difference of two Timestamps results in a TimeDelta.
class Timestamp {
 public:
  Timestamp() : Timestamp(units_internal::kNotInitializedVal) {}
  static Timestamp Infinity() {
    return Timestamp(units_internal::kPlusInfinityVal);
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
  int64_t s() const { return units_internal::DivideAndRound(us(), 1000000); }
  int64_t ms() const { return units_internal::DivideAndRound(us(), 1000); }
  int64_t us() const {
    RTC_DCHECK(IsFinite());
    return microseconds_;
  }
  bool IsInfinite() const {
    return microseconds_ == units_internal::kPlusInfinityVal;
  }
  bool IsInitialized() const {
    return microseconds_ != units_internal::kNotInitializedVal;
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

  double SecondsAsDouble() const;
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

// DataSize is a class represeting a count of bytes. Note that while it can be
// initialized by a number of bits, it does not guarantee that the resolution is
// kept and the internal storage is in bytes. The number of bits will be
// truncated to fit.
class DataSize {
 public:
  DataSize() : DataSize(units_internal::kNotInitializedVal) {}
  static DataSize Zero() { return DataSize(0); }
  static DataSize Infinity() {
    return DataSize(units_internal::kPlusInfinityVal);
  }
  static DataSize bytes(int64_t bytes) {
    RTC_DCHECK_GE(bytes, 0);
    return DataSize(bytes);
  }
  static DataSize bits(int64_t bits) {
    RTC_DCHECK_GE(bits, 0);
    return DataSize(bits / 8);
  }
  int64_t bytes() const {
    RTC_DCHECK(IsFinite());
    return bytes_;
  }
  int64_t kilobytes() const {
    return units_internal::DivideAndRound(bytes(), 1000);
  }
  int64_t bits() const { return bytes() * 8; }
  int64_t kilobits() const {
    return units_internal::DivideAndRound(bits(), 1000);
  }
  bool IsZero() const { return bytes_ == 0; }
  bool IsInfinite() const { return bytes_ == units_internal::kPlusInfinityVal; }
  bool IsInitialized() const {
    return bytes_ != units_internal::kNotInitializedVal;
  }
  bool IsFinite() const { return IsInitialized() && !IsInfinite(); }
  DataSize operator-(const DataSize& other) const {
    return DataSize::bytes(bytes() - other.bytes());
  }
  DataSize operator+(const DataSize& other) const {
    return DataSize::bytes(bytes() + other.bytes());
  }
  DataSize operator*(double scalar) const;
  DataSize operator*(int64_t scalar) const {
    return DataSize::bytes(bytes() * scalar);
  }
  DataSize operator*(int32_t scalar) const {
    return DataSize::bytes(bytes() * scalar);
  }
  DataSize operator/(int64_t scalar) const {
    return DataSize::bytes(bytes() / scalar);
  }
  DataSize& operator-=(const DataSize& other) {
    bytes_ -= other.bytes();
    return *this;
  }
  DataSize& operator+=(const DataSize& other) {
    bytes_ += other.bytes();
    return *this;
  }
  bool operator==(const DataSize& other) const {
    return bytes_ == other.bytes_;
  }
  bool operator!=(const DataSize& other) const {
    return bytes_ != other.bytes_;
  }
  bool operator<=(const DataSize& other) const {
    return bytes_ <= other.bytes_;
  }
  bool operator>=(const DataSize& other) const {
    return bytes_ >= other.bytes_;
  }
  bool operator>(const DataSize& other) const { return bytes_ > other.bytes_; }
  bool operator<(const DataSize& other) const { return bytes_ < other.bytes_; }

 private:
  explicit DataSize(int64_t bytes) : bytes_(bytes) {}
  int64_t bytes_;
};
inline DataSize operator*(const double& scalar, const DataSize& size) {
  return size * scalar;
}
inline DataSize operator*(const int64_t& scalar, const DataSize& size) {
  return size * scalar;
}
inline DataSize operator*(const int32_t& scalar, const DataSize& size) {
  return size * scalar;
}

// DataRate is a class that represents a given data rate. This can be used to
// represent bandwidth, encoding bitrate, etc. The internal storage is currently
// bits per second (bps) since this makes it easier to intepret the raw value
// when debugging. The promised precision, however is only that it will
// represent bytes per second accurately. Any implementation depending on bps
// resolution should document this by changing this comment.
class DataRate {
 public:
  DataRate() : DataRate(units_internal::kNotInitializedVal) {}
  static DataRate Zero() { return DataRate(0); }
  static DataRate Infinity() {
    return DataRate(units_internal::kPlusInfinityVal);
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
  int64_t kbps() const { return units_internal::DivideAndRound(bps(), 1000); }
  bool IsZero() const { return bits_per_sec_ == 0; }
  bool IsInfinite() const {
    return bits_per_sec_ == units_internal::kPlusInfinityVal;
  }
  bool IsInitialized() const {
    return bits_per_sec_ != units_internal::kNotInitializedVal;
  }
  bool IsFinite() const { return IsInitialized() && !IsInfinite(); }
  DataRate operator*(double scalar) const;
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

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_INCLUDE_NETWORK_UNITS_H_
