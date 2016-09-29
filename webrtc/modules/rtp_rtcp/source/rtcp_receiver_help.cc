/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_receiver_help.h"

#include <assert.h>  // assert
#include <string.h>  // memset

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"

namespace webrtc {
namespace RTCPHelp {

RTCPReportBlockInformation::RTCPReportBlockInformation()
    : remoteReceiveBlock(),
      remoteMaxJitter(0),
      RTT(0),
      minRTT(0),
      maxRTT(0),
      avgRTT(0),
      numAverageCalcs(0) {
  memset(&remoteReceiveBlock, 0, sizeof(remoteReceiveBlock));
}

RTCPReportBlockInformation::~RTCPReportBlockInformation() {}

}  // namespace RTCPHelp
}  // namespace webrtc
