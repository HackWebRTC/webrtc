/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_TEST_NETWORK_OSTREAM_OPERATORS_H_
#define MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_TEST_NETWORK_OSTREAM_OPERATORS_H_
// Overloading ostream << operator is required for gtest printing.
#include <ostream>  // no-presubmit-check TODO(webrtc:8982)
#include "modules/congestion_controller/network_control/include/network_types.h"

namespace webrtc {
::std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    ::std::ostream& os,      // no-presubmit-check TODO(webrtc:8982)
    const DataRate& datarate);
::std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    ::std::ostream& os,      // no-presubmit-check TODO(webrtc:8982)
    const DataSize& datasize);
::std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    ::std::ostream& os,      // no-presubmit-check TODO(webrtc:8982)
    const Timestamp& timestamp);
::std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    ::std::ostream& os,      // no-presubmit-check TODO(webrtc:8982)
    const TimeDelta& delta);

::std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    ::std::ostream& os,      // no-presubmit-check TODO(webrtc:8982)
    const CongestionWindow& window);
::std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    ::std::ostream& os,      // no-presubmit-check TODO(webrtc:8982)
    const ProbeClusterConfig& config);
::std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    ::std::ostream& os,      // no-presubmit-check TODO(webrtc:8982)
    const PacerConfig& config);
::std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    ::std::ostream& os,      // no-presubmit-check TODO(webrtc:8982)
    const TargetTransferRate& target_rate);
}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_TEST_NETWORK_OSTREAM_OPERATORS_H_
