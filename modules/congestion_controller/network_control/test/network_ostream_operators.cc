/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/network_control/test/network_ostream_operators.h"

namespace webrtc {
::std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    ::std::ostream& os,      // no-presubmit-check TODO(webrtc:8982)
    const CongestionWindow& window) {
  return os << "CongestionWindow(...)";
}
::std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    ::std::ostream& os,      // no-presubmit-check TODO(webrtc:8982)
    const ProbeClusterConfig& config) {
  return os << "ProbeClusterConfig(...)";
}
::std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    ::std::ostream& os,      // no-presubmit-check TODO(webrtc:8982)
    const PacerConfig& config) {
  return os << "PacerConfig(...)";
}
::std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    ::std::ostream& os,      // no-presubmit-check TODO(webrtc:8982)
    const TargetTransferRate& target_rate) {
  return os << "TargetTransferRate(...)";
}
}  // namespace webrtc
