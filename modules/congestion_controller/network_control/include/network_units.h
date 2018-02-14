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
#include <ostream>
#include "rtc_base/checks.h"

namespace webrtc {
namespace units_internal {
inline int64_t DivideAndRound(int64_t numerator, int64_t denominators) {
  if (numerator >= 0) {
    return (numerator + (denominators / 2)) / denominators;
  } else {
    return (numerator + (denominators / 2)) / denominators - 1;
  }
}
}  // namespace units_internal

// TimeDelta represents the difference between two timestamps. Connomly this can
// be a duration. However since two Timestamps are not guaranteed to have the
// same epoch (they might come from different computers, making exact
// synchronisation infeasible), the duration covered by a TimeDelta can be
// undefined. To simplify usage, it can be constructed and converted to
// different units, specifically seconds (s), milliseconds (ms) and
// microseconds (us).
class TimeDelta {
 public:
  static const TimeDelta kPlusInfinity;
  static const TimeDelta kMinusInfinity;
  static const TimeDelta kNotInitialized;
  static const TimeDelta kZero;
  TimeDelta() : TimeDelta(kNotInitialized) {}
  static TimeDelta Zero() { return kZero; }
  static TimeDelta Infinity() { return kPlusInfinity; }
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
  bool IsZero() const { return microseconds_ == 0; }
  bool IsFinite() const { return IsInitialized() && !IsInfinite(); }
  bool IsInitialized() const {
    return microseconds_ != kNotInitialized.microseconds_;
  }
  bool IsInfinite() const {
    return *this == kPlusInfinity || *this == kMinusInfinity;
  }
  TimeDelta operator+(const TimeDelta& other) const {
    return TimeDelta::us(us() + other.us());
  }
  TimeDelta operator-(const TimeDelta& other) const {
    return TimeDelta::us(us() - other.us());
  }
  TimeDelta operator*(double scalar) const {
    return TimeDelta::us(us() * scalar);
  }
  TimeDelta operator*(int64_t scalar) const {
    return TimeDelta::us(us() * scalar);
  }
  TimeDelta operator*(int32_t scalar) const {
    return TimeDelta::us(us() * scalar);
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
// difference of of two Timestamps results in a TimeDelta.
class Timestamp {
 public:
  static const Timestamp kPlusInfinity;
  static const Timestamp kNotInitialized;
  Timestamp() : Timestamp(kNotInitialized) {}
  static Timestamp Infinity() { return kPlusInfinity; }
  static Timestamp s(int64_t seconds) { return Timestamp(seconds * 1000000); }
  static Timestamp ms(int64_t millis) { return Timestamp(millis * 1000); }
  static Timestamp us(int64_t micros) { return Timestamp(micros); }
  int64_t s() const { return units_internal::DivideAndRound(us(), 1000000); }
  int64_t ms() const { return units_internal::DivideAndRound(us(), 1000); }
  int64_t us() const {
    RTC_DCHECK(IsFinite());
    return microseconds_;
  }
  bool IsInfinite() const {
    return microseconds_ == kPlusInfinity.microseconds_;
  }
  bool IsInitialized() const {
    return microseconds_ != kNotInitialized.microseconds_;
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
  static const DataSize kZero;
  static const DataSize kPlusInfinity;
  static const DataSize kNotInitialized;
  DataSize() : DataSize(kNotInitialized) {}
  static DataSize Zero() { return kZero; }
  static DataSize Infinity() { return kPlusInfinity; }
  static DataSize bytes(int64_t bytes) { return DataSize(bytes); }
  static DataSize bits(int64_t bits) { return DataSize(bits / 8); }
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
  bool IsInfinite() const { return bytes_ == kPlusInfinity.bytes_; }
  bool IsInitialized() const { return bytes_ != kNotInitialized.bytes_; }
  bool IsFinite() const { return IsInitialized() && !IsInfinite(); }
  DataSize operator-(const DataSize& other) const {
    return DataSize::bytes(bytes() - other.bytes());
  }
  DataSize operator+(const DataSize& other) const {
    return DataSize::bytes(bytes() + other.bytes());
  }
  DataSize operator*(double scalar) const {
    return DataSize::bytes(bytes() * scalar);
  }
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
  static const DataRate kZero;
  static const DataRate kPlusInfinity;
  static const DataRate kNotInitialized;
  DataRate() : DataRate(kNotInitialized) {}
  static DataRate Zero() { return kZero; }
  static DataRate Infinity() { return kPlusInfinity; }
  static DataRate bytes_per_second(int64_t bytes_per_sec) {
    return DataRate(bytes_per_sec * 8);
  }
  static DataRate bits_per_second(int64_t bits_per_sec) {
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
  int64_t kbps() const { return units_internal::DivideAndRound(bps(), 1000); }
  bool IsZero() const { return bits_per_sec_ == 0; }
  bool IsInfinite() const {
    return bits_per_sec_ == kPlusInfinity.bits_per_sec_;
  }
  bool IsInitialized() const {
    return bits_per_sec_ != kNotInitialized.bits_per_sec_;
  }
  bool IsFinite() const { return IsInitialized() && !IsInfinite(); }
  DataRate operator*(double scalar) const {
    return DataRate::bytes_per_second(bytes_per_second() * scalar);
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

::std::ostream& operator<<(::std::ostream& os, const DataRate& datarate);
::std::ostream& operator<<(::std::ostream& os, const DataSize& datasize);
::std::ostream& operator<<(::std::ostream& os, const Timestamp& timestamp);
::std::ostream& operator<<(::std::ostream& os, const TimeDelta& delta);

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_INCLUDE_NETWORK_UNITS_H_
