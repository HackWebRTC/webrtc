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

#include "api/call/transport.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sdes.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "modules/rtp_rtcp/source/time_util.h"
#include "rtc_base/checks.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/task_queue.h"

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
    : config_(config), ptr_factory_(this) {
  RTC_CHECK(config_.Validate());
  if (config_.schedule_periodic_compound_packets)
    ReschedulePeriodicCompoundPackets(config_.initial_report_delay_ms);
}

RtcpTransceiverImpl::~RtcpTransceiverImpl() = default;

void RtcpTransceiverImpl::ReceivePacket(rtc::ArrayView<const uint8_t> packet) {
  while (!packet.empty()) {
    rtcp::CommonHeader rtcp_block;
    if (!rtcp_block.Parse(packet.data(), packet.size()))
      return;

    HandleReceivedPacket(rtcp_block);

    // TODO(danilchap): Use packet.remove_prefix() when that function exists.
    packet = packet.subview(rtcp_block.packet_size());
  }
}

void RtcpTransceiverImpl::SendCompoundPacket() {
  SendPacket();
  if (config_.schedule_periodic_compound_packets)
    ReschedulePeriodicCompoundPackets(config_.report_period_ms);
}

void RtcpTransceiverImpl::SetRemb(int bitrate_bps,
                                  std::vector<uint32_t> ssrcs) {
  RTC_DCHECK_GE(bitrate_bps, 0);
  remb_.emplace();
  remb_->SetSsrcs(std::move(ssrcs));
  remb_->SetBitrateBps(bitrate_bps);
  // TODO(bugs.webrtc.org/8239): Move logic from PacketRouter for sending remb
  // immideately on large bitrate change when there is one RtcpTransceiver per
  // rtp transport.
}

void RtcpTransceiverImpl::UnsetRemb() {
  remb_.reset();
}

void RtcpTransceiverImpl::HandleReceivedPacket(
    const rtcp::CommonHeader& rtcp_packet_header) {
  switch (rtcp_packet_header.type()) {
    case rtcp::SenderReport::kPacketType: {
      rtcp::SenderReport sender_report;
      if (!sender_report.Parse(rtcp_packet_header))
        return;
      SenderReportTimes& last =
          last_received_sender_reports_[sender_report.sender_ssrc()];
      last.local_received_time_us = rtc::TimeMicros();
      last.remote_sent_time = sender_report.ntp();
      break;
    }
  }
}

void RtcpTransceiverImpl::ReschedulePeriodicCompoundPackets(int64_t delay_ms) {
  class SendPeriodicCompoundPacket : public rtc::QueuedTask {
   public:
    SendPeriodicCompoundPacket(rtc::TaskQueue* task_queue,
                               rtc::WeakPtr<RtcpTransceiverImpl> ptr)
        : task_queue_(task_queue), ptr_(std::move(ptr)) {}
    bool Run() override {
      RTC_DCHECK(task_queue_->IsCurrent());
      if (!ptr_)
        return true;
      ptr_->SendPacket();
      task_queue_->PostDelayedTask(rtc::WrapUnique(this),
                                   ptr_->config_.report_period_ms);
      return false;
    }

   private:
    rtc::TaskQueue* const task_queue_;
    const rtc::WeakPtr<RtcpTransceiverImpl> ptr_;
  };

  RTC_DCHECK(config_.schedule_periodic_compound_packets);
  RTC_DCHECK(config_.task_queue->IsCurrent());

  // Stop existent send task if there is one.
  ptr_factory_.InvalidateWeakPtrs();
  auto task = rtc::MakeUnique<SendPeriodicCompoundPacket>(
      config_.task_queue, ptr_factory_.GetWeakPtr());
  if (delay_ms > 0)
    config_.task_queue->PostDelayedTask(std::move(task), delay_ms);
  else
    config_.task_queue->PostTask(std::move(task));
}

void RtcpTransceiverImpl::SendPacket() {
  PacketSender sender(config_.outgoing_transport, config_.max_packet_size);
  const uint32_t sender_ssrc = config_.feedback_ssrc;

  rtcp::ReceiverReport receiver_report;
  receiver_report.SetSenderSsrc(sender_ssrc);
  receiver_report.SetReportBlocks(CreateReportBlocks());
  sender.AppendPacket(receiver_report);

  if (!config_.cname.empty()) {
    rtcp::Sdes sdes;
    bool added = sdes.AddCName(config_.feedback_ssrc, config_.cname);
    RTC_DCHECK(added) << "Failed to add cname " << config_.cname
                      << " to rtcp sdes packet.";
    sender.AppendPacket(sdes);
  }
  if (remb_) {
    remb_->SetSenderSsrc(sender_ssrc);
    sender.AppendPacket(*remb_);
  }

  sender.Send();
}

std::vector<rtcp::ReportBlock> RtcpTransceiverImpl::CreateReportBlocks() {
  if (!config_.receive_statistics)
    return {};
  // TODO(danilchap): Support sending more than
  // |ReceiverReport::kMaxNumberOfReportBlocks| per compound rtcp packet.
  std::vector<rtcp::ReportBlock> report_blocks =
      config_.receive_statistics->RtcpReportBlocks(
          rtcp::ReceiverReport::kMaxNumberOfReportBlocks);
  for (rtcp::ReportBlock& report_block : report_blocks) {
    auto it = last_received_sender_reports_.find(report_block.source_ssrc());
    if (it == last_received_sender_reports_.end())
      continue;
    const SenderReportTimes& last_sender_report = it->second;
    report_block.SetLastSr(CompactNtp(last_sender_report.remote_sent_time));
    report_block.SetDelayLastSr(SaturatedUsToCompactNtp(
        rtc::TimeMicros() - last_sender_report.local_received_time_us));
  }
  return report_blocks;
}

}  // namespace webrtc
