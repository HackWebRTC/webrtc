/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_transceiver_impl.h"

#include <utility>
#include <vector>

#include "api/call/transport.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

// Helper to put several RTCP packets into lower layer datagram composing
// Compound or Reduced-Size RTCP packet, as defined by RFC 5506 section 2.
class PacketSender : public rtcp::RtcpPacket::PacketReadyCallback {
 public:
  PacketSender(Transport* transport, size_t max_packet_size)
      : transport_(transport), max_packet_size_(max_packet_size) {
    RTC_CHECK_LE(max_packet_size, IP_PACKET_SIZE);
  }
  ~PacketSender() override {
    RTC_DCHECK_EQ(index_, 0) << "Unsent rtcp packet.";
  }

  // Appends a packet to pending compound packet.
  // Sends rtcp compound packet if buffer was already full and resets buffer.
  void AppendPacket(const rtcp::RtcpPacket& packet) {
    packet.Create(buffer_, &index_, max_packet_size_, this);
  }

  // Sends pending rtcp compound packet.
  void Send() {
    if (index_ > 0) {
      OnPacketReady(buffer_, index_);
      index_ = 0;
    }
  }

 private:
  // Implements RtcpPacket::PacketReadyCallback
  void OnPacketReady(uint8_t* data, size_t length) override {
    transport_->SendRtcp(data, length);
  }

  Transport* const transport_;
  const size_t max_packet_size_;
  size_t index_ = 0;
  uint8_t buffer_[IP_PACKET_SIZE];
};

}  // namespace

RtcpTransceiverImpl::RtcpTransceiverImpl(const RtcpTransceiverConfig& config)
    : config_(config) {
  RTC_CHECK(config_.Validate());
}

RtcpTransceiverImpl::~RtcpTransceiverImpl() = default;

void RtcpTransceiverImpl::SendCompoundPacket() {
  PacketSender sender(config_.outgoing_transport, config_.max_packet_size);

  rtcp::ReceiverReport rr;
  rr.SetSenderSsrc(config_.feedback_ssrc);
  if (config_.receive_statistics) {
    // TODO(danilchap): Support sending more than
    // |ReceiverReport::kMaxNumberOfReportBlocks| per compound rtcp packet.
    std::vector<rtcp::ReportBlock> report_blocks =
        config_.receive_statistics->RtcpReportBlocks(
            rtcp::ReceiverReport::kMaxNumberOfReportBlocks);
    // TODO(danilchap): Fill in LastSr/DelayLastSr fields of report blocks
    // when RtcpTransceiver handles incoming sender reports.
    rr.SetReportBlocks(std::move(report_blocks));
  }
  sender.AppendPacket(rr);
  // TODO(danilchap): Append SDES to conform to the requirements on minimal
  // compound RTCP packet.

  sender.Send();
}

}  // namespace webrtc
