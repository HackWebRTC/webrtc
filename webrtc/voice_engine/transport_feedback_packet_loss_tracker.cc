/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/transport_feedback_packet_loss_tracker.h"

#include <limits>
#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/base/mod_ops.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"

namespace {
constexpr uint16_t kSeqNumHalf = 0x8000u;
constexpr uint16_t kSeqNumQuarter = kSeqNumHalf / 2;
constexpr size_t kMaxConsecutiveOldReports = 4;

void UpdateCounter(size_t* counter, bool increment) {
  if (increment) {
    RTC_DCHECK_LT(*counter, std::numeric_limits<std::size_t>::max());
    ++(*counter);
  } else {
    RTC_DCHECK_GT(*counter, 0);
    --(*counter);
  }
}

}  // namespace

namespace webrtc {

TransportFeedbackPacketLossTracker::TransportFeedbackPacketLossTracker(
    size_t max_window_size,
    size_t plr_min_num_packets,
    size_t rplr_min_num_pairs)
    : max_window_size_(max_window_size),
      ref_packet_status_(packet_status_window_.begin()),
      plr_state_(plr_min_num_packets),
      rplr_state_(rplr_min_num_pairs) {
  RTC_DCHECK_GT(plr_min_num_packets, 0);
  RTC_DCHECK_GE(max_window_size, plr_min_num_packets);
  RTC_DCHECK_LE(max_window_size, kSeqNumHalf);
  RTC_DCHECK_GT(rplr_min_num_pairs, 0);
  RTC_DCHECK_GT(max_window_size, rplr_min_num_pairs);
  Reset();
}

void TransportFeedbackPacketLossTracker::Reset() {
  plr_state_.Reset();
  rplr_state_.Reset();
  num_consecutive_old_reports_ = 0;
  packet_status_window_.clear();
  ref_packet_status_ = packet_status_window_.begin();
}

uint16_t TransportFeedbackPacketLossTracker::ReferenceSequenceNumber() const {
  RTC_DCHECK(!packet_status_window_.empty());
  return ref_packet_status_->first;
}

bool TransportFeedbackPacketLossTracker::IsOldSequenceNumber(
    uint16_t seq_num) const {
  if (packet_status_window_.empty()) {
    return false;
  }
  const uint16_t diff = ForwardDiff(ReferenceSequenceNumber(), seq_num);
  return diff >= 3 * kSeqNumQuarter;
}

void TransportFeedbackPacketLossTracker::OnReceivedTransportFeedback(
    const rtcp::TransportFeedback& feedback) {
  const auto& fb_vector = feedback.GetStatusVector();
  const uint16_t base_seq_num = feedback.GetBaseSequence();

  if (IsOldSequenceNumber(base_seq_num)) {
    ++num_consecutive_old_reports_;
    if (num_consecutive_old_reports_ <= kMaxConsecutiveOldReports) {
      // If the number consecutive old reports have not exceed a threshold, we
      // consider this packet as a late arrival. We could consider adding it to
      // |packet_status_window_|, but in current implementation, we simply
      // ignore it.
      return;
    }
    // If we see several consecutive older reports, we assume that we've not
    // received reports for an exceedingly long time, and do a reset.
    Reset();
    RTC_DCHECK(!IsOldSequenceNumber(base_seq_num));
  } else {
    num_consecutive_old_reports_ = 0;
  }

  uint16_t seq_num = base_seq_num;
  for (size_t i = 0; i < fb_vector.size(); ++i, ++seq_num) {
    // Remove the oldest feedbacks so that the distance between the oldest and
    // the packet to be added does not exceed or equal to half of total sequence
    // numbers.
    while (!packet_status_window_.empty() &&
           ForwardDiff(ReferenceSequenceNumber(), seq_num) >= kSeqNumHalf) {
      RemoveOldestPacketStatus();
    }

    const bool received =
        fb_vector[i] !=
        webrtc::rtcp::TransportFeedback::StatusSymbol::kNotReceived;
    InsertPacketStatus(seq_num, received);

    while (packet_status_window_.size() > max_window_size_) {
      // Make sure that the window holds at most |max_window_size_| items.
      RemoveOldestPacketStatus();
    }
  }
}

rtc::Optional<float>
TransportFeedbackPacketLossTracker::GetPacketLossRate() const {
  return plr_state_.GetMetric();
}

rtc::Optional<float>
TransportFeedbackPacketLossTracker::GetRecoverablePacketLossRate() const {
  return rplr_state_.GetMetric();
}

void TransportFeedbackPacketLossTracker::InsertPacketStatus(uint16_t seq_num,
                                                            bool received) {
  const auto& ret =
      packet_status_window_.insert(std::make_pair(seq_num, received));
  if (!ret.second) {
    if (!ret.first->second && received) {
      // If older status said that the packet was lost but newer one says it
      // is received, we take the newer one.
      UpdateMetrics(ret.first, false);
      ret.first->second = received;
    } else {
      // If the value is unchanged or if older status said that the packet was
      // received but the newer one says it is lost, we ignore it.
      return;
    }
  }
  UpdateMetrics(ret.first, true);
  if (packet_status_window_.size() == 1)
    ref_packet_status_ = ret.first;
}

void TransportFeedbackPacketLossTracker::RemoveOldestPacketStatus() {
  UpdateMetrics(ref_packet_status_, false);
  const auto it = ref_packet_status_;
  ref_packet_status_ = NextPacketStatus(it);
  packet_status_window_.erase(it);
}

void TransportFeedbackPacketLossTracker::UpdateMetrics(
    PacketStatusIterator it,
    bool apply /* false = undo */) {
  RTC_DCHECK(it != packet_status_window_.end());
  UpdatePlr(it, apply);
  UpdateRplr(it, apply);
}

void TransportFeedbackPacketLossTracker::UpdatePlr(
    PacketStatusIterator it,
    bool apply /* false = undo */) {
  // Record or undo reception status of currently handled packet.
  if (it->second) {
    UpdateCounter(&plr_state_.num_received_packets_, apply);
  } else {
    UpdateCounter(&plr_state_.num_lost_packets_, apply);
  }
}

void TransportFeedbackPacketLossTracker::UpdateRplr(
    PacketStatusIterator it,
    bool apply /* false = undo */) {
  // Previous packet and current packet might compose a known pair.
  // If so, the RPLR state needs to be updated accordingly.
  if (it != ref_packet_status_) {
    const auto& prev = PreviousPacketStatus(it);
    if (prev->first == static_cast<uint16_t>(it->first - 1)) {
      UpdateCounter(&rplr_state_.num_known_pairs_, apply);
      if (!prev->second && it->second) {
        UpdateCounter(
            &rplr_state_.num_recoverable_losses_, apply);
      }
    }
  }

  // Current packet and next packet might compose a pair.
  // If so, the RPLR state needs to be updated accordingly.
  const auto& next = NextPacketStatus(it);
  if (next != packet_status_window_.end() &&
      next->first == static_cast<uint16_t>(it->first + 1)) {
    UpdateCounter(&rplr_state_.num_known_pairs_, apply);
    if (!it->second && next->second) {
      UpdateCounter(&rplr_state_.num_recoverable_losses_, apply);
    }
  }
}

TransportFeedbackPacketLossTracker::PacketStatusIterator
TransportFeedbackPacketLossTracker::PreviousPacketStatus(
    PacketStatusIterator it) {
  RTC_DCHECK(it != ref_packet_status_);
  if (it == packet_status_window_.end()) {
    // This is to make PreviousPacketStatus(packet_status_window_.end()) point
    // to the last element.
    it = ref_packet_status_;
  }

  if (it == packet_status_window_.begin()) {
    // Due to the circular nature of sequence numbers, we let the iterator
    // go to the end.
    it = packet_status_window_.end();
  }
  return --it;
}

TransportFeedbackPacketLossTracker::PacketStatusIterator
TransportFeedbackPacketLossTracker::NextPacketStatus(PacketStatusIterator it) {
  RTC_DCHECK(it != packet_status_window_.end());
  ++it;
  if (it == packet_status_window_.end()) {
    // Due to the circular nature of sequence numbers, we let the iterator
    // goes back to the beginning.
    it = packet_status_window_.begin();
  }
  if (it == ref_packet_status_) {
    // This is to make the NextPacketStatus of the last element to return the
    // beyond-the-end iterator.
    it = packet_status_window_.end();
  }
  return it;
}

// TODO(minyue): This method checks the states of this class do not misbehave.
// The method is used both in unit tests and a fuzzer test. The fuzzer test
// is present to help finding potential errors. Once the fuzzer test shows no
// error after long period, we can remove the fuzzer test, and move this method
// to unit test.
void TransportFeedbackPacketLossTracker::Validate() const {  // Testing only!
  RTC_CHECK_LE(packet_status_window_.size(), max_window_size_);
  RTC_CHECK_EQ(packet_status_window_.size(),
               plr_state_.num_lost_packets_ + plr_state_.num_received_packets_);
  RTC_CHECK_LE(rplr_state_.num_recoverable_losses_,
               rplr_state_.num_known_pairs_);
  RTC_CHECK_LE(rplr_state_.num_known_pairs_,
               packet_status_window_.size() - 1);

  size_t received_packets = 0;
  size_t lost_packets = 0;
  size_t known_status_pairs = 0;
  size_t recoverable_losses = 0;

  if (!packet_status_window_.empty()) {
    PacketStatusIterator it = ref_packet_status_;
    do {
      if (it->second) {
        ++received_packets;
      } else {
        ++lost_packets;
      }

      auto next = std::next(it);
      if (next == packet_status_window_.end())
        next = packet_status_window_.begin();

      if (next != ref_packet_status_ &&
          next->first == static_cast<uint16_t>(it->first + 1)) {
        ++known_status_pairs;
        if (!it->second && next->second)
          ++recoverable_losses;
      }

      RTC_CHECK_LT(ForwardDiff(ReferenceSequenceNumber(), it->first),
                   kSeqNumHalf);

      it = next;
    } while (it != ref_packet_status_);
  }

  RTC_CHECK_EQ(plr_state_.num_received_packets_, received_packets);
  RTC_CHECK_EQ(plr_state_.num_lost_packets_, lost_packets);
  RTC_CHECK_EQ(rplr_state_.num_known_pairs_, known_status_pairs);
  RTC_CHECK_EQ(rplr_state_.num_recoverable_losses_, recoverable_losses);
}

rtc::Optional<float>
TransportFeedbackPacketLossTracker::PlrState::GetMetric() const {
  const size_t total = num_lost_packets_ + num_received_packets_;
  if (total < min_num_packets_) {
    return rtc::Optional<float>();
  } else {
    return rtc::Optional<float>(
        static_cast<float>(num_lost_packets_) / total);
  }
}

rtc::Optional<float>
TransportFeedbackPacketLossTracker::RplrState::GetMetric() const {
  if (num_known_pairs_ < min_num_pairs_) {
    return rtc::Optional<float>();
  } else {
    return rtc::Optional<float>(
        static_cast<float>(num_recoverable_losses_) / num_known_pairs_);
  }
}

}  // namespace webrtc
