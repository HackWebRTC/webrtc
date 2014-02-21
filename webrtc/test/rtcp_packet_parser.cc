/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/rtcp_packet_parser.h"

namespace webrtc {
namespace test {

RtcpPacketParser::RtcpPacketParser() {}

RtcpPacketParser::~RtcpPacketParser() {}

void RtcpPacketParser::Parse(const void *data, int len) {
  const uint8_t* packet = static_cast<const uint8_t*>(data);
  RTCPUtility::RTCPParserV2 parser(packet, len, true);
  for (RTCPUtility::RTCPPacketTypes type = parser.Begin();
      type != RTCPUtility::kRtcpNotValidCode;
      type = parser.Iterate()) {
    if (type == RTCPUtility::kRtcpSrCode) {
      sender_report_.Set(parser.Packet().SR);
    } else if (type == RTCPUtility::kRtcpRrCode) {
      receiver_report_.Set(parser.Packet().RR);
    } else if (type == RTCPUtility::kRtcpByeCode) {
      bye_.Set(parser.Packet().BYE);
    } else if (type == RTCPUtility::kRtcpReportBlockItemCode) {
      report_block_.Set(parser.Packet().ReportBlockItem);
      ++report_blocks_per_ssrc_[parser.Packet().ReportBlockItem.SSRC];
    } else if (type == RTCPUtility::kRtcpPsfbFirCode) {
      fir_.Set(parser.Packet().FIR);
    } else if (type == webrtc::RTCPUtility::kRtcpPsfbFirItemCode) {
      fir_item_.Set(parser.Packet().FIRItem);
    } else if (type == RTCPUtility::kRtcpRtpfbNackCode) {
      nack_.Set(parser.Packet().NACK);
    } else if (type == RTCPUtility::kRtcpRtpfbNackItemCode) {
      nack_item_.Set(parser.Packet().NACKItem);
    }
  }
}
}  // namespace test
}  // namespace webrtc
