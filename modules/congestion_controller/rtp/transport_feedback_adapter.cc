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

#include "absl/algorithm/container.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

constexpr TimeDelta kSendTimeHistoryWindow = TimeDelta::Seconds<60>();

void InFlightBytesTracker::AddInFlightPacketBytes(
    const PacketFeedback& packet) {
  RTC_DCHECK(packet.sent.send_time.IsFinite());
  auto it = in_flight_data_.find({packet.local_net_id, packet.remote_net_id});
  if (it != in_flight_data_.end()) {
    it->second += packet.sent.size;
  } else {
    in_flight_data_.insert(
        {{packet.local_net_id, packet.remote_net_id}, packet.sent.size});
  }
}

void InFlightBytesTracker::RemoveInFlightPacketBytes(
    const PacketFeedback& packet) {
  if (packet.sent.send_time.IsInfinite())
    return;
  auto it = in_flight_data_.find({packet.local_net_id, packet.remote_net_id});
  if (it != in_flight_data_.end()) {
    RTC_DCHECK_GE(it->second, packet.sent.size);
    it->second -= packet.sent.size;
    if (it->second.IsZero())
      in_flight_data_.erase(it);
  }
}

DataSize InFlightBytesTracker::GetOutstandingData(
    uint16_t local_net_id,
    uint16_t remote_net_id) const {
  auto it = in_flight_data_.find({local_net_id, remote_net_id});
  if (it != in_flight_data_.end()) {
    return it->second;
  } else {
    return DataSize::Zero();
  }
}

TransportFeedbackAdapter::TransportFeedbackAdapter() = default;

TransportFeedbackAdapter::~TransportFeedbackAdapter() {
  RTC_DCHECK(observers_.empty());
}

void TransportFeedbackAdapter::RegisterStreamFeedbackObserver(
    std::vector<uint32_t> ssrcs,
    StreamFeedbackObserver* observer) {
  rtc::CritScope cs(&observers_lock_);
  RTC_DCHECK(observer);
  RTC_DCHECK(absl::c_find_if(observers_, [=](const auto& pair) {
               return pair.second == observer;
             }) == observers_.end());
  observers_.push_back({ssrcs, observer});
}

void TransportFeedbackAdapter::DeRegisterStreamFeedbackObserver(
    StreamFeedbackObserver* observer) {
  rtc::CritScope cs(&observers_lock_);
  RTC_DCHECK(observer);
  const auto it = absl::c_find_if(
      observers_, [=](const auto& pair) { return pair.second == observer; });
  RTC_DCHECK(it != observers_.end());
  observers_.erase(it);
}

void TransportFeedbackAdapter::AddPacket(const RtpPacketSendInfo& packet_info,
                                         size_t overhead_bytes,
                                         Timestamp creation_time) {
  {
    rtc::CritScope cs(&lock_);
    PacketFeedback packet;
    packet.creation_time = creation_time;
    packet.sent.sequence_number =
        seq_num_unwrapper_.Unwrap(packet_info.transport_sequence_number);
    packet.sent.size = DataSize::bytes(packet_info.length + overhead_bytes);
    packet.local_net_id = local_net_id_;
    packet.remote_net_id = remote_net_id_;
    packet.sent.pacing_info = packet_info.pacing_info;
    if (packet_info.has_rtp_sequence_number) {
      packet.ssrc = packet_info.ssrc;
      packet.rtp_sequence_number = packet_info.rtp_sequence_number;
    }

    while (!history_.empty() &&
           creation_time - history_.begin()->second.creation_time >
               kSendTimeHistoryWindow) {
      // TODO(sprang): Warn if erasing (too many) old items?
      if (history_.begin()->second.sent.sequence_number > last_ack_seq_num_)
        in_flight_.RemoveInFlightPacketBytes(history_.begin()->second);
      history_.erase(history_.begin());
    }
    history_.insert(std::make_pair(packet.sent.sequence_number, packet));
  }
}
absl::optional<SentPacket> TransportFeedbackAdapter::ProcessSentPacket(
    const rtc::SentPacket& sent_packet) {
  rtc::CritScope cs(&lock_);
  auto send_time = Timestamp::ms(sent_packet.send_time_ms);
  // TODO(srte): Only use one way to indicate that packet feedback is used.
  if (sent_packet.info.included_in_feedback || sent_packet.packet_id != -1) {
    int64_t unwrapped_seq_num =
        seq_num_unwrapper_.Unwrap(sent_packet.packet_id);
    auto it = history_.find(unwrapped_seq_num);
    if (it != history_.end()) {
      bool packet_retransmit = it->second.sent.send_time.IsFinite();
      it->second.sent.send_time = send_time;
      last_send_time_ = std::max(last_send_time_, send_time);
      // TODO(srte): Don't do this on retransmit.
      if (!pending_untracked_size_.IsZero()) {
        if (send_time < last_untracked_send_time_)
          RTC_LOG(LS_WARNING)
              << "appending acknowledged data for out of order packet. (Diff: "
              << ToString(last_untracked_send_time_ - send_time) << " ms.)";
        it->second.sent.prior_unacked_data += pending_untracked_size_;
        pending_untracked_size_ = DataSize::Zero();
      }
      if (!packet_retransmit) {
        if (it->second.sent.sequence_number > last_ack_seq_num_)
          in_flight_.AddInFlightPacketBytes(it->second);
        it->second.sent.data_in_flight = GetOutstandingData();
        return it->second.sent;
      }
    }
  } else if (sent_packet.info.included_in_allocation) {
    if (send_time < last_send_time_) {
      RTC_LOG(LS_WARNING) << "ignoring untracked data for out of order packet.";
    }
    pending_untracked_size_ +=
        DataSize::bytes(sent_packet.info.packet_size_bytes);
    last_untracked_send_time_ = std::max(last_untracked_send_time_, send_time);
  }
  return absl::nullopt;
}

absl::optional<TransportPacketsFeedback>
TransportFeedbackAdapter::ProcessTransportFeedback(
    const rtcp::TransportFeedback& feedback,
    Timestamp feedback_receive_time) {
  if (feedback.GetPacketStatusCount() == 0) {
    RTC_LOG(LS_INFO) << "Empty transport feedback packet received.";
    return absl::nullopt;
  }
  std::vector<PacketFeedback> feedback_vector;
  TransportPacketsFeedback msg;
  msg.feedback_time = feedback_receive_time;
  {
    rtc::CritScope cs(&lock_);
    msg.prior_in_flight =
        in_flight_.GetOutstandingData(local_net_id_, remote_net_id_);
    feedback_vector =
        ProcessTransportFeedbackInner(feedback, feedback_receive_time);
    if (feedback_vector.empty())
      return absl::nullopt;

    for (const PacketFeedback& fb : feedback_vector) {
      PacketResult res;
      res.sent_packet = fb.sent;
      res.receive_time = fb.receive_time;
      msg.packet_feedbacks.push_back(res);
    }
    auto it = history_.find(last_ack_seq_num_);
    if (it != history_.end()) {
      msg.first_unacked_send_time = it->second.sent.send_time;
    }
    msg.data_in_flight =
        in_flight_.GetOutstandingData(local_net_id_, remote_net_id_);
  }
  SignalObservers(feedback_vector);
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
  return in_flight_.GetOutstandingData(local_net_id_, remote_net_id_);
}

std::vector<PacketFeedback>
TransportFeedbackAdapter::ProcessTransportFeedbackInner(
    const rtcp::TransportFeedback& feedback,
    Timestamp feedback_time) {
  // Add timestamp deltas to a local time base selected on first packet arrival.
  // This won't be the true time base, but makes it easier to manually inspect
  // time stamps.
  if (last_timestamp_.IsInfinite()) {
    current_offset_ = feedback_time;
  } else {
    // TODO(srte): We shouldn't need to do rounding here.
    current_offset_ += feedback.GetBaseDelta(last_timestamp_)
                           .RoundDownTo(TimeDelta::Millis<1>());
  }
  last_timestamp_ = feedback.GetBaseTime();

  std::vector<PacketFeedback> packet_feedback_vector;
  packet_feedback_vector.reserve(feedback.GetPacketStatusCount());

  size_t failed_lookups = 0;
  size_t ignored = 0;
  TimeDelta packet_offset = TimeDelta::Zero();
  for (const auto& packet : feedback.GetAllPackets()) {
    int64_t seq_num = seq_num_unwrapper_.Unwrap(packet.sequence_number());

    if (seq_num > last_ack_seq_num_) {
      // Starts at history_.begin() if last_ack_seq_num_ < 0, since any valid
      // sequence number is >= 0.
      for (auto it = history_.upper_bound(last_ack_seq_num_);
           it != history_.upper_bound(seq_num); ++it) {
        in_flight_.RemoveInFlightPacketBytes(it->second);
      }
      last_ack_seq_num_ = seq_num;
    }

    auto it = history_.find(seq_num);
    if (it == history_.end()) {
      ++failed_lookups;
      continue;
    }

    if (it->second.sent.send_time.IsInfinite()) {
      // TODO(srte): Fix the tests that makes this happen and make this a
      // DCHECK.
      RTC_DLOG(LS_ERROR)
          << "Received feedback before packet was indicated as sent";
      continue;
    }

    PacketFeedback packet_feedback = it->second;
    if (packet.received()) {
      packet_offset += packet.delta();
      packet_feedback.receive_time =
          current_offset_ + packet_offset.RoundDownTo(TimeDelta::Millis<1>());
      // Note: Lost packets are not removed from history because they might be
      // reported as received by a later feedback.
      history_.erase(it);
    }
    if (packet_feedback.local_net_id == local_net_id_ &&
        packet_feedback.remote_net_id == remote_net_id_) {
      packet_feedback_vector.push_back(packet_feedback);
    } else {
      ++ignored;
    }
  }

  if (failed_lookups > 0) {
    RTC_LOG(LS_WARNING) << "Failed to lookup send time for " << failed_lookups
                        << " packet" << (failed_lookups > 1 ? "s" : "")
                        << ". Send time history too small?";
  }
  if (ignored > 0) {
    RTC_LOG(LS_INFO) << "Ignoring " << ignored
                     << " packets because they were sent on a different route.";
  }

  return packet_feedback_vector;
}

void TransportFeedbackAdapter::SignalObservers(
    const std::vector<PacketFeedback>& feedback_vector) {
  rtc::CritScope cs(&observers_lock_);
  for (auto& observer : observers_) {
    std::vector<StreamFeedbackObserver::StreamPacketInfo> selected_feedback;
    for (const auto& packet : feedback_vector) {
      if (packet.ssrc && absl::c_count(observer.first, *packet.ssrc) > 0) {
        StreamFeedbackObserver::StreamPacketInfo packet_info;
        packet_info.ssrc = *packet.ssrc;
        packet_info.rtp_sequence_number = packet.rtp_sequence_number;
        packet_info.received = packet.receive_time.IsFinite();
        selected_feedback.push_back(packet_info);
      }
    }
    if (!selected_feedback.empty()) {
      observer.second->OnPacketFeedbackVector(std::move(selected_feedback));
    }
  }
}

}  // namespace webrtc
