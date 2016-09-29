/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_RECEIVER_HELP_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_RECEIVER_HELP_H_

#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace RTCPHelp {

class RTCPReportBlockInformation {
 public:
  RTCPReportBlockInformation();
  ~RTCPReportBlockInformation();

  // Statistics
  RTCPReportBlock remoteReceiveBlock;
  uint32_t remoteMaxJitter;

  // RTT
  int64_t RTT;
  int64_t minRTT;
  int64_t maxRTT;
  int64_t avgRTT;
  uint32_t numAverageCalcs;
};

}  // end namespace RTCPHelp
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_RECEIVER_HELP_H_
