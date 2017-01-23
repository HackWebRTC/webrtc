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
}  // namespace

namespace webrtc {

TransportFeedbackPacketLossTracker::TransportFeedbackPacketLossTracker(
    size_t min_window_size,
    size_t max_window_size)
    : min_window_size_(min_window_size),
      max_window_size_(max_window_size),
      ref_packet_status_(packet_status_window_.begin()) {
  RTC_DCHECK_GT(min_window_size, 0);
  RTC_DCHECK_GE(max_window_size_, min_window_size_);
  RTC_DCHECK_LE(max_window_size_, kSeqNumHalf);
  Reset();
}

void TransportFeedbackPacketLossTracker::Reset() {
  num_received_packets_ = 0;
  num_lost_packets_ = 0;
  num_consecutive_losses_ = 0;
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

bool TransportFeedbackPacketLossTracker::GetPacketLossRates(
    float* packet_loss_rate,
    float* consecutive_packet_loss_rate) const {
  const size_t total = num_lost_packets_ + num_received_packets_;
  if (total < min_window_size_)
    return false;
  *packet_loss_rate = static_cast<float>(num_lost_packets_) / total;
  *consecutive_packet_loss_rate =
      static_cast<float>(num_consecutive_losses_) / total;
  return true;
}

void TransportFeedbackPacketLossTracker::InsertPacketStatus(uint16_t seq_num,
                                                            bool received) {
  const auto& ret =
      packet_status_window_.insert(std::make_pair(seq_num, received));
  if (!ret.second) {
    if (!ret.first->second && received) {
      // If older status said that the packet was lost but newer one says it
      // is received, we take the newer one.
      UndoPacketStatus(ret.first);
      ret.first->second = received;
    } else {
      // If the value is unchanged or if older status said that the packet was
      // received but the newer one says it is lost, we ignore it.
      return;
    }
  }
  ApplyPacketStatus(ret.first);
  if (packet_status_window_.size() == 1)
    ref_packet_status_ = ret.first;
}

void TransportFeedbackPacketLossTracker::RemoveOldestPacketStatus() {
  UndoPacketStatus(ref_packet_status_);
  const auto it = ref_packet_status_;
  ref_packet_status_ = NextPacketStatus(it);
  packet_status_window_.erase(it);
}

void TransportFeedbackPacketLossTracker::ApplyPacketStatus(
    PacketStatusIterator it) {
  RTC_DCHECK(it != packet_status_window_.end());
  if (it->second) {
    ++num_received_packets_;
  } else {
    ++num_lost_packets_;
    const auto& next = NextPacketStatus(it);
    if (next != packet_status_window_.end() &&
        next->first == static_cast<uint16_t>(it->first + 1) && !next->second) {
      // Feedback shows that the next packet has been lost. Since this
      // packet is lost, we increase the consecutive loss counter.
      ++num_consecutive_losses_;
    }
    if (it != ref_packet_status_) {
      const auto& pre = PreviousPacketStatus(it);
      if (pre->first == static_cast<uint16_t>(it->first - 1) && !pre->second) {
        // Feedback shows that the previous packet has been lost. Since this
        // packet is lost, we increase the consecutive loss counter.
        ++num_consecutive_losses_;
      }
    }
  }
}

void TransportFeedbackPacketLossTracker::UndoPacketStatus(
    PacketStatusIterator it) {
  RTC_DCHECK(it != packet_status_window_.end());
  if (it->second) {
    RTC_DCHECK_GT(num_received_packets_, 0);
    --num_received_packets_;
  } else {
    RTC_DCHECK_GT(num_lost_packets_, 0);
    --num_lost_packets_;
    const auto& next = NextPacketStatus(it);
    if (next != packet_status_window_.end() &&
        next->first == static_cast<uint16_t>(it->first + 1) && !next->second) {
      RTC_DCHECK_GT(num_consecutive_losses_, 0);
      --num_consecutive_losses_;
    }
    if (it != ref_packet_status_) {
      const auto& pre = PreviousPacketStatus(it);
      if (pre->first == static_cast<uint16_t>(it->first - 1) && !pre->second) {
        RTC_DCHECK_GT(num_consecutive_losses_, 0);
        --num_consecutive_losses_;
      }
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
  RTC_CHECK_GE(num_lost_packets_, num_consecutive_losses_);
  RTC_CHECK_EQ(packet_status_window_.size(),
               num_lost_packets_ + num_received_packets_);

  size_t received_packets = 0;
  size_t lost_packets = 0;
  size_t consecutive_losses = 0;

  if (!packet_status_window_.empty()) {
    PacketStatusIterator it = ref_packet_status_;
    bool pre_lost = false;
    uint16_t pre_seq_num = it->first - 1;
    do {
      if (it->second) {
        ++received_packets;
      } else {
        ++lost_packets;
        if (pre_lost && pre_seq_num == static_cast<uint16_t>(it->first - 1))
          ++consecutive_losses;
      }

      RTC_CHECK_LT(ForwardDiff(ReferenceSequenceNumber(), it->first),
                   kSeqNumHalf);

      pre_lost = !it->second;
      pre_seq_num = it->first;

      ++it;
      if (it == packet_status_window_.end())
        it = packet_status_window_.begin();
    } while (it != ref_packet_status_);
  }

  RTC_CHECK_EQ(num_received_packets_, received_packets);
  RTC_CHECK_EQ(num_lost_packets_, lost_packets);
  RTC_CHECK_EQ(num_consecutive_losses_, consecutive_losses);
}

}  // namespace webrtc
