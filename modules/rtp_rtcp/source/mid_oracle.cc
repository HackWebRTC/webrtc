/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/mid_oracle.h"

namespace webrtc {

MidOracle::MidOracle(const std::string& mid) : mid_(mid) {}

MidOracle::~MidOracle() = default;

void MidOracle::OnReceivedRtcpReportBlocks(
    const ReportBlockList& report_blocks) {
  if (!send_mid_) {
    return;
  }
  for (const RTCPReportBlock& report_block : report_blocks) {
    if (report_block.source_ssrc == ssrc_) {
      send_mid_ = false;
      break;
    }
  }
}

}  // namespace webrtc
