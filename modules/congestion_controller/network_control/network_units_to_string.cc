/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/network_control/include/network_units_to_string.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
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
std::string ToString(const DataSize& value) {
  char buf[64];
  rtc::SimpleStringBuilder sb(buf);
  if (value.IsInfinite()) {
    sb << "inf bytes";
  } else if (!value.IsInitialized()) {
    sb << "? bytes";
  } else {
    sb << value.bytes() << " bytes";
  }
  return sb.str();
}
std::string ToString(const Timestamp& value) {
  char buf[64];
  rtc::SimpleStringBuilder sb(buf);
  if (value.IsInfinite()) {
    sb << "inf ms";
  } else if (!value.IsInitialized()) {
    sb << "? ms";
  } else {
    sb << value.ms() << " ms";
  }
  return sb.str();
}
std::string ToString(const TimeDelta& value) {
  char buf[64];
  rtc::SimpleStringBuilder sb(buf);
  if (value.IsPlusInfinity()) {
    sb << "+inf ms";
  } else if (value.IsMinusInfinity()) {
    sb << "-inf ms";
  } else if (!value.IsInitialized()) {
    sb << "? ms";
  } else {
    sb << value.ms() << " ms";
  }
  return sb.str();
}
}  // namespace webrtc
