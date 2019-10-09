/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/rtp/transport_feedback_adapter.h"

#include <stdlib.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {

PacketResult NetworkPacketFeedbackFromRtpPacketFeedback(
    const webrtc::PacketFeedback& pf) {
  PacketResult feedback;
  if (pf.arrival_time_ms == webrtc::PacketFeedback::kNotReceived) {
    feedback.receive_time = Timestamp::PlusInfinity();
  } else {
    feedback.receive_time = Timestamp::ms(pf.arrival_time_ms);
  }
  feedback.sent_packet.sequence_number = pf.long_sequence_number;
  feedback.sent_packet.send_time = Timestamp::ms(pf.send_time_ms);
  feedback.sent_packet.size = DataSize::bytes(pf.payload_size);
  feedback.sent_packet.pacing_info = pf.pacing_info;
  feedback.sent_packet.prior_unacked_data =
      DataSize::bytes(pf.unacknowledged_data);
  return feedback;
}
}  // namespace
const int64_t kNoTimestamp = -1;
const int64_t kSendTimeHistoryWindowMs = 60000;

TransportFeedbackAdapter::TransportFeedbackAdapter()
    : packet_age_limit_ms_(kSendTimeHistoryWindowMs),
      current_offset_ms_(kNoTimestamp),
      last_timestamp_us_(kNoTimestamp),
      local_net_id_(0),
      remote_net_id_(0) {}

TransportFeedbackAdapter::~TransportFeedbackAdapter() {
  RTC_DCHECK(observers_.empty());
}

void TransportFeedbackAdapter::RegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  rtc::CritScope cs(&observers_lock_);
  RTC_DCHECK(observer);
  RTC_DCHECK(std::find(observers_.begin(), observers_.end(), observer) ==
             observers_.end());
  observers_.push_back(observer);
}

void TransportFeedbackAdapter::DeRegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  rtc::CritScope cs(&observers_lock_);
  RTC_DCHECK(observer);
  const auto it = std::find(observers_.begin(), observers_.end(), observer);
  RTC_DCHECK(it != observers_.end());
  observers_.erase(it);
}

void TransportFeedbackAdapter::AddPacket(const RtpPacketSendInfo& packet_info,
                                         size_t overhead_bytes,
                                         Timestamp creation_time) {
  {
    rtc::CritScope cs(&lock_);
    PacketFeedback packet(creation_time.ms(),
                          packet_info.transport_sequence_number,
                          packet_info.length + overhead_bytes, local_net_id_,
                          remote_net_id_, packet_info.pacing_info);
    if (packet_info.has_rtp_sequence_number) {
      packet.ssrc = packet_info.ssrc;
      packet.rtp_sequence_number = packet_info.rtp_sequence_number;
    }
    packet.long_sequence_number =
        seq_num_unwrapper_.Unwrap(packet.sequence_number);

    while (!history_.empty() &&
           creation_time.ms() - history_.begin()->second.creation_time_ms >
               packet_age_limit_ms_) {
      // TODO(sprang): Warn if erasing (too many) old items?
      RemoveInFlightPacketBytes(history_.begin()->second);
      history_.erase(history_.begin());
    }
    history_.insert(std::make_pair(packet.long_sequence_number, packet));
  }

  {
    rtc::CritScope cs(&observers_lock_);
    for (auto* observer : observers_) {
      observer->OnPacketAdded(packet_info.ssrc,
                              packet_info.transport_sequence_number);
    }
  }
}
absl::optional<SentPacket> TransportFeedbackAdapter::ProcessSentPacket(
    const rtc::SentPacket& sent_packet) {
  rtc::CritScope cs(&lock_);
  // TODO(srte): Only use one way to indicate that packet feedback is used.
  if (sent_packet.info.included_in_feedback || sent_packet.packet_id != -1) {
    int64_t unwrapped_seq_num =
        seq_num_unwrapper_.Unwrap(sent_packet.packet_id);
    auto it = history_.find(unwrapped_seq_num);
    if (it != history_.end()) {
      bool packet_retransmit = it->second.send_time_ms >= 0;
      it->second.send_time_ms = sent_packet.send_time_ms;
      last_send_time_ms_ =
          std::max(last_send_time_ms_, sent_packet.send_time_ms);
      // TODO(srte): Don't do this on retransmit.
      if (pending_untracked_size_ > 0) {
        if (sent_packet.send_time_ms < last_untracked_send_time_ms_)
          RTC_LOG(LS_WARNING)
              << "appending acknowledged data for out of order packet. (Diff: "
              << last_untracked_send_time_ms_ - sent_packet.send_time_ms
              << " ms.)";
        it->second.unacknowledged_data += pending_untracked_size_;
        pending_untracked_size_ = 0;
      }
      if (!packet_retransmit) {
        AddInFlightPacketBytes(it->second);
        auto packet = it->second;
        SentPacket msg;
        msg.size = DataSize::bytes(packet.payload_size);
        msg.send_time = Timestamp::ms(packet.send_time_ms);
        msg.sequence_number = packet.long_sequence_number;
        msg.prior_unacked_data = DataSize::bytes(packet.unacknowledged_data);
        msg.data_in_flight = GetOutstandingData();
        return msg;
      }
    }
  } else if (sent_packet.info.included_in_allocation) {
    if (sent_packet.send_time_ms < last_send_time_ms_) {
      RTC_LOG(LS_WARNING) << "ignoring untracked data for out of order packet.";
    }
    pending_untracked_size_ += sent_packet.info.packet_size_bytes;
    last_untracked_send_time_ms_ =
        std::max(last_untracked_send_time_ms_, sent_packet.send_time_ms);
  }
  return absl::nullopt;
}

absl::optional<TransportPacketsFeedback>
TransportFeedbackAdapter::ProcessTransportFeedback(
    const rtcp::TransportFeedback& feedback,
    Timestamp feedback_receive_time) {
  DataSize prior_in_flight = GetOutstandingData();

  last_packet_feedback_vector_ =
      GetPacketFeedbackVector(feedback, feedback_receive_time);
  {
    rtc::CritScope cs(&observers_lock_);
    for (auto* observer : observers_) {
      observer->OnPacketFeedbackVector(last_packet_feedback_vector_);
    }
  }

  std::vector<PacketFeedback> feedback_vector = last_packet_feedback_vector_;
  if (feedback_vector.empty())
    return absl::nullopt;

  TransportPacketsFeedback msg;
  for (const PacketFeedback& rtp_feedback : feedback_vector) {
    if (rtp_feedback.send_time_ms != PacketFeedback::kNoSendTime) {
      auto feedback = NetworkPacketFeedbackFromRtpPacketFeedback(rtp_feedback);
      msg.packet_feedbacks.push_back(feedback);
    } else if (rtp_feedback.arrival_time_ms == PacketFeedback::kNotReceived) {
      msg.sendless_arrival_times.push_back(Timestamp::PlusInfinity());
    } else {
      msg.sendless_arrival_times.push_back(
          Timestamp::ms(rtp_feedback.arrival_time_ms));
    }
  }
  {
    rtc::CritScope cs(&lock_);
    auto it = history_.find(last_ack_seq_num_);
    if (it != history_.end() &&
        it->second.send_time_ms != PacketFeedback::kNoSendTime) {
      msg.first_unacked_send_time = Timestamp::ms(it->second.send_time_ms);
    }
  }
  msg.feedback_time = feedback_receive_time;
  msg.prior_in_flight = prior_in_flight;
  msg.data_in_flight = GetOutstandingData();
  return msg;
}

void TransportFeedbackAdapter::SetNetworkIds(uint16_t local_id,
                                             uint16_t remote_id) {
  rtc::CritScope cs(&lock_);
  local_net_id_ = local_id;
  remote_net_id_ = remote_id;
}

DataSize TransportFeedbackAdapter::GetOutstandingData() const {
  rtc::CritScope cs(&lock_);
  auto it = in_flight_bytes_.find({local_net_id_, remote_net_id_});
  if (it != in_flight_bytes_.end()) {
    return DataSize::bytes(it->second);
  } else {
    return DataSize::Zero();
  }
}

std::vector<PacketFeedback> TransportFeedbackAdapter::GetPacketFeedbackVector(
    const rtcp::TransportFeedback& feedback,
    Timestamp feedback_time) {
  // Add timestamp deltas to a local time base selected on first packet arrival.
  // This won't be the true time base, but makes it easier to manually inspect
  // time stamps.
  if (last_timestamp_us_ == kNoTimestamp) {
    current_offset_ms_ = feedback_time.ms();
  } else {
    current_offset_ms_ += feedback.GetBaseDeltaUs(last_timestamp_us_) / 1000;
  }
  last_timestamp_us_ = feedback.GetBaseTimeUs();

  std::vector<PacketFeedback> packet_feedback_vector;
  if (feedback.GetPacketStatusCount() == 0) {
    RTC_LOG(LS_INFO) << "Empty transport feedback packet received.";
    return packet_feedback_vector;
  }
  packet_feedback_vector.reserve(feedback.GetPacketStatusCount());
  {
    rtc::CritScope cs(&lock_);
    size_t failed_lookups = 0;
    int64_t offset_us = 0;
    int64_t timestamp_ms = 0;
    uint16_t seq_num = feedback.GetBaseSequence();
    for (const auto& packet : feedback.GetReceivedPackets()) {
      // Insert into the vector those unreceived packets which precede this
      // iteration's received packet.
      for (; seq_num != packet.sequence_number(); ++seq_num) {
        PacketFeedback packet_feedback(PacketFeedback::kNotReceived, seq_num);
        // Note: Element not removed from history because it might be reported
        // as received by another feedback.
        if (!GetFeedback(&packet_feedback, false))
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
      if (!GetFeedback(&packet_feedback, true))
        ++failed_lookups;
      if (packet_feedback.local_net_id == local_net_id_ &&
          packet_feedback.remote_net_id == remote_net_id_) {
        packet_feedback_vector.push_back(packet_feedback);
      }

      ++seq_num;
    }

    if (failed_lookups > 0) {
      RTC_LOG(LS_WARNING) << "Failed to lookup send time for " << failed_lookups
                          << " packet" << (failed_lookups > 1 ? "s" : "")
                          << ". Send time history too small?";
    }
  }
  return packet_feedback_vector;
}

std::vector<PacketFeedback>
TransportFeedbackAdapter::GetTransportFeedbackVector() const {
  return last_packet_feedback_vector_;
}

bool TransportFeedbackAdapter::GetFeedback(PacketFeedback* packet_feedback,
                                           bool remove) {
  RTC_DCHECK(packet_feedback);
  int64_t acked_seq_num =
      seq_num_unwrapper_.Unwrap(packet_feedback->sequence_number);

  if (acked_seq_num > last_ack_seq_num_) {
    // Returns history_.begin() if last_ack_seq_num_ < 0, since any valid
    // sequence number is >= 0.
    auto unacked_it = history_.lower_bound(last_ack_seq_num_);
    auto newly_acked_end = history_.upper_bound(acked_seq_num);
    for (; unacked_it != newly_acked_end; ++unacked_it) {
      RemoveInFlightPacketBytes(unacked_it->second);
    }
    last_ack_seq_num_ = acked_seq_num;
  }

  auto it = history_.find(acked_seq_num);
  if (it == history_.end())
    return false;

  // Save arrival_time not to overwrite it.
  int64_t arrival_time_ms = packet_feedback->arrival_time_ms;
  *packet_feedback = it->second;
  packet_feedback->arrival_time_ms = arrival_time_ms;

  if (remove)
    history_.erase(it);
  return true;
}

void TransportFeedbackAdapter::AddInFlightPacketBytes(
    const PacketFeedback& packet) {
  RTC_DCHECK_NE(packet.send_time_ms, -1);
  if (last_ack_seq_num_ >= packet.long_sequence_number)
    return;
  auto it = in_flight_bytes_.find({packet.local_net_id, packet.remote_net_id});
  if (it != in_flight_bytes_.end()) {
    it->second += packet.payload_size;
  } else {
    in_flight_bytes_[{packet.local_net_id, packet.remote_net_id}] =
        packet.payload_size;
  }
}

void TransportFeedbackAdapter::RemoveInFlightPacketBytes(
    const PacketFeedback& packet) {
  if (packet.send_time_ms < 0 ||
      last_ack_seq_num_ >= packet.long_sequence_number)
    return;
  auto it = in_flight_bytes_.find({packet.local_net_id, packet.remote_net_id});
  if (it != in_flight_bytes_.end()) {
    it->second -= packet.payload_size;
    if (it->second == 0)
      in_flight_bytes_.erase(it);
  }
}

}  // namespace webrtc
