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

#include "webrtc/base/constructormagic.h"
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
  static const uint8_t kPacketType = 207;

  ExtendedReports();
  ~ExtendedReports() override;

  // Parse assumes header is already parsed and validated.
  bool Parse(const RTCPUtility::RtcpCommonHeader& header,
             const uint8_t* payload);  // Size of the payload is in the header.

  void From(uint32_t ssrc) { sender_ssrc_ = ssrc; }

  // Max 50 items of each of {Rrtr, Dlrr, VoipMetric} allowed per Xr.
  bool WithRrtr(const Rrtr& rrtr);
  bool WithDlrr(const Dlrr& dlrr);
  bool WithVoipMetric(const VoipMetric& voip_metric);

  uint32_t sender_ssrc() const { return sender_ssrc_; }
  const std::vector<Rrtr>& rrtrs() const { return rrtr_blocks_; }
  const std::vector<Dlrr>& dlrrs() const { return dlrr_blocks_; }
  const std::vector<VoipMetric>& voip_metrics() const {
    return voip_metric_blocks_;
  }

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  static const size_t kMaxNumberOfRrtrBlocks = 50;
  static const size_t kMaxNumberOfDlrrBlocks = 50;
  static const size_t kMaxNumberOfVoipMetricBlocks = 50;
  static const size_t kXrBaseLength = 4;

  size_t BlockLength() const override {
    return kHeaderLength + kXrBaseLength + RrtrLength() + DlrrLength() +
           VoipMetricLength();
  }

  size_t RrtrLength() const { return Rrtr::kLength * rrtr_blocks_.size(); }
  size_t DlrrLength() const;
  size_t VoipMetricLength() const {
    return VoipMetric::kLength * voip_metric_blocks_.size();
  }

  void ParseRrtrBlock(const uint8_t* block, uint16_t block_length);
  void ParseDlrrBlock(const uint8_t* block, uint16_t block_length);
  void ParseVoipMetricBlock(const uint8_t* block, uint16_t block_length);

  uint32_t sender_ssrc_;
  std::vector<Rrtr> rrtr_blocks_;
  std::vector<Dlrr> dlrr_blocks_;
  std::vector<VoipMetric> voip_metric_blocks_;

  RTC_DISALLOW_COPY_AND_ASSIGN(ExtendedReports);
};
}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_EXTENDED_REPORTS_H_
