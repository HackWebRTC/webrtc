/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_EXTENDED_REPORTS_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_EXTENDED_REPORTS_H_

#include <vector>

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/dlrr.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/rrtr.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/voip_metric.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"

namespace webrtc {
namespace rtcp {

// From RFC 3611: RTP Control Protocol Extended Reports (RTCP XR).
class ExtendedReports : public RtcpPacket {
 public:
  typedef std::vector<RTCPUtility::RTCPPacketXRDLRRReportBlockItem> DlrrBlock;
  ExtendedReports() : RtcpPacket() {
    memset(&xr_header_, 0, sizeof(xr_header_));
  }

  virtual ~ExtendedReports() {}

  void From(uint32_t ssrc) {
    xr_header_.OriginatorSSRC = ssrc;
  }

  // Max 50 items of each of {Rrtr, Dlrr, VoipMetric} allowed per Xr.
  bool WithRrtr(Rrtr* rrtr);
  bool WithDlrr(Dlrr* dlrr);
  bool WithVoipMetric(VoipMetric* voip_metric);

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  static const int kMaxNumberOfRrtrBlocks = 50;
  static const int kMaxNumberOfDlrrBlocks = 50;
  static const int kMaxNumberOfVoipMetricBlocks = 50;

  size_t BlockLength() const {
    const size_t kXrHeaderLength = 8;
    return kXrHeaderLength + RrtrLength() + DlrrLength() + VoipMetricLength();
  }

  size_t RrtrLength() const { return Rrtr::kLength * rrtr_blocks_.size(); }

  size_t DlrrLength() const;

  size_t VoipMetricLength() const {
    return VoipMetric::kLength * voip_metric_blocks_.size();
  }

  RTCPUtility::RTCPPacketXR xr_header_;
  std::vector<Rrtr> rrtr_blocks_;
  std::vector<Dlrr> dlrr_blocks_;
  std::vector<VoipMetric> voip_metric_blocks_;

  RTC_DISALLOW_COPY_AND_ASSIGN(ExtendedReports);
};

}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_EXTENDED_REPORTS_H_
