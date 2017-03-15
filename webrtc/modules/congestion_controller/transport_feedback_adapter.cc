/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/congestion_controller/transport_feedback_adapter.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/mod_ops.h"
#include "webrtc/modules/congestion_controller/delay_based_bwe.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "webrtc/system_wrappers/include/field_trial.h"

namespace webrtc {

const int64_t kNoTimestamp = -1;
const int64_t kSendTimeHistoryWindowMs = 60000;
const int64_t kBaseTimestampScaleFactor =
    rtcp::TransportFeedback::kDeltaScaleFactor * (1 << 8);
const int64_t kBaseTimestampRangeSizeUs = kBaseTimestampScaleFactor * (1 << 24);

TransportFeedbackAdapter::TransportFeedbackAdapter(const Clock* clock)
    : send_side_bwe_with_overhead_(
          webrtc::field_trial::IsEnabled("WebRTC-SendSideBwe-WithOverhead")),
      transport_overhead_bytes_per_packet_(0),
      send_time_history_(clock, kSendTimeHistoryWindowMs),
      clock_(clock),
      current_offset_ms_(kNoTimestamp),
      last_timestamp_us_(kNoTimestamp),
      local_net_id_(0),
      remote_net_id_(0) {}

TransportFeedbackAdapter::~TransportFeedbackAdapter() {}

void TransportFeedbackAdapter::AddPacket(uint16_t sequence_number,
                                         size_t length,
                                         const PacedPacketInfo& pacing_info) {
  rtc::CritScope cs(&lock_);
  if (send_side_bwe_with_overhead_) {
    length += transport_overhead_bytes_per_packet_;
  }
  const int64_t creation_time_ms = clock_->TimeInMilliseconds();
  send_time_history_.AddAndRemoveOld(
      PacketFeedback(creation_time_ms, sequence_number, length, local_net_id_,
                     remote_net_id_, pacing_info));
}

void TransportFeedbackAdapter::OnSentPacket(uint16_t sequence_number,
                                            int64_t send_time_ms) {
  rtc::CritScope cs(&lock_);
  send_time_history_.OnSentPacket(sequence_number, send_time_ms);
}

void TransportFeedbackAdapter::SetTransportOverhead(
    int transport_overhead_bytes_per_packet) {
  rtc::CritScope cs(&lock_);
  transport_overhead_bytes_per_packet_ = transport_overhead_bytes_per_packet;
}

void TransportFeedbackAdapter::SetNetworkIds(uint16_t local_id,
                                             uint16_t remote_id) {
  rtc::CritScope cs(&lock_);
  local_net_id_ = local_id;
  remote_net_id_ = remote_id;
}

std::vector<PacketFeedback> TransportFeedbackAdapter::GetPacketFeedbackVector(
    const rtcp::TransportFeedback& feedback) {
  int64_t timestamp_us = feedback.GetBaseTimeUs();
  // Add timestamp deltas to a local time base selected on first packet arrival.
  // This won't be the true time base, but makes it easier to manually inspect
  // time stamps.
  if (last_timestamp_us_ == kNoTimestamp) {
    current_offset_ms_ = clock_->TimeInMilliseconds();
  } else {
    int64_t delta = timestamp_us - last_timestamp_us_;

    // Detect and compensate for wrap-arounds in base time.
    if (std::abs(delta - kBaseTimestampRangeSizeUs) < std::abs(delta)) {
      delta -= kBaseTimestampRangeSizeUs;  // Wrap backwards.
    } else if (std::abs(delta + kBaseTimestampRangeSizeUs) < std::abs(delta)) {
      delta += kBaseTimestampRangeSizeUs;  // Wrap forwards.
    }

    current_offset_ms_ += delta / 1000;
  }
  last_timestamp_us_ = timestamp_us;

  auto received_packets = feedback.GetReceivedPackets();
  std::vector<PacketFeedback> packet_feedback_vector;
  if (received_packets.empty()) {
    LOG(LS_INFO) << "Empty transport feedback packet received.";
    return packet_feedback_vector;
  }
  const uint16_t last_sequence_number =
      received_packets.back().sequence_number();
  const size_t packet_count =
      1 + ForwardDiff(feedback.GetBaseSequence(), last_sequence_number);
  packet_feedback_vector.reserve(packet_count);
  // feedback.GetStatusVector().size() is a less efficient way to reach what
  // should be the same value.
  RTC_DCHECK_EQ(packet_count, feedback.GetStatusVector().size());

  {
    rtc::CritScope cs(&lock_);
    size_t failed_lookups = 0;
    int64_t offset_us = 0;
    int64_t timestamp_ms = 0;
    uint16_t seq_num = feedback.GetBaseSequence();
    for (const auto& packet : received_packets) {
      // Insert into the vector those unreceived packets which precede this
      // iteration's received packet.
      for (; seq_num != packet.sequence_number(); ++seq_num) {
        PacketFeedback packet_feedback(PacketFeedback::kNotReceived, seq_num);
        // Note: Element not removed from history because it might be reported
        // as received by another feedback.
        if (!send_time_history_.GetFeedback(&packet_feedback, false))
          ++failed_lookups;
        if (packet_feedback.local_net_id == local_net_id_ &&
            packet_feedback.remote_net_id == remote_net_id_) {
          packet_feedback_vector.push_back(packet_feedback);
        }
      }

      // Handle this iteration's received packet.
      offset_us += packet.delta_us();
      timestamp_ms = current_offset_ms_ + (offset_us / 1000);
      PacketFeedback packet_feedback(timestamp_ms, packet.sequence_number());
      if (!send_time_history_.GetFeedback(&packet_feedback, true))
        ++failed_lookups;
      if (packet_feedback.local_net_id == local_net_id_ &&
          packet_feedback.remote_net_id == remote_net_id_) {
        packet_feedback_vector.push_back(packet_feedback);
      }

      ++seq_num;
    }

    if (failed_lookups > 0) {
      LOG(LS_WARNING) << "Failed to lookup send time for " << failed_lookups
                      << " packet" << (failed_lookups > 1 ? "s" : "")
                      << ". Send time history too small?";
    }
  }
  return packet_feedback_vector;
}

void TransportFeedbackAdapter::OnTransportFeedback(
    const rtcp::TransportFeedback& feedback) {
  last_packet_feedback_vector_ = GetPacketFeedbackVector(feedback);
}

std::vector<PacketFeedback>
TransportFeedbackAdapter::GetTransportFeedbackVector() const {
  return last_packet_feedback_vector_;
}
}  // namespace webrtc
