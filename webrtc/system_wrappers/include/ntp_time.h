/*
*  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/
#ifndef WEBRTC_SYSTEM_WRAPPERS_INCLUDE_NTP_TIME_H_
#define WEBRTC_SYSTEM_WRAPPERS_INCLUDE_NTP_TIME_H_

#include <stdint.h>

namespace webrtc {

class NtpTime {
 public:
  NtpTime() : seconds_(0), fractions_(0) {}
  NtpTime(uint32_t seconds, uint32_t fractions)
      : seconds_(seconds), fractions_(fractions) {}

  NtpTime(const NtpTime&) = default;
  NtpTime& operator=(const NtpTime&) = default;

  void Set(uint32_t seconds, uint32_t fractions) {
    seconds_ = seconds;
    fractions_ = fractions;
  }
  void Reset() {
    seconds_ = 0;
    fractions_ = 0;
  }

  int64_t ToMs() const {
    static constexpr double kNtpFracPerMs = 4.294967296E6;  // 2^32 / 1000.
    const double frac_ms = static_cast<double>(fractions_) / kNtpFracPerMs;
    return 1000 * static_cast<int64_t>(seconds_) +
           static_cast<int64_t>(frac_ms + 0.5);
  }
  // NTP standard (RFC1305, section 3.1) explicitly state value 0/0 is invalid.
  bool Valid() const { return !(seconds_ == 0 && fractions_ == 0); }

  uint32_t seconds() const { return seconds_; }
  uint32_t fractions() const { return fractions_; }

 private:
  uint32_t seconds_;
  uint32_t fractions_;
};

inline bool operator==(const NtpTime& n1, const NtpTime& n2) {
  return n1.seconds() == n2.seconds() && n1.fractions() == n2.fractions();
}
inline bool operator!=(const NtpTime& n1, const NtpTime& n2) {
  return !(n1 == n2);
}

}  // namespace webrtc
#endif  // WEBRTC_SYSTEM_WRAPPERS_INCLUDE_NTP_TIME_H_
