/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_packet_history.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "absl/memory/memory.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace {
// Min packet size for BestFittingPacket() to honor.
constexpr size_t kMinPacketRequestBytes = 50;

// Utility function to get the absolute difference in size between the provided
// target size and the size of packet.
size_t SizeDiff(size_t packet_size, size_t size) {
  if (packet_size > size) {
    return packet_size - size;
  }
  return size - packet_size;
}
}  // namespace

constexpr size_t RtpPacketHistory::kMaxCapacity;
constexpr int64_t RtpPacketHistory::kMinPacketDurationMs;
constexpr int RtpPacketHistory::kMinPacketDurationRtt;
constexpr int RtpPacketHistory::kPacketCullingDelayFactor;

RtpPacketHistory::PacketState::PacketState() = default;
RtpPacketHistory::PacketState::PacketState(const PacketState&) = default;
RtpPacketHistory::PacketState::~PacketState() = default;

RtpPacketHistory::StoredPacket::StoredPacket(
    std::unique_ptr<RtpPacketToSend> packet,
    StorageType storage_type,
    absl::optional<int64_t> send_time_ms,
    uint64_t insert_order)
    : send_time_ms_(send_time_ms),
      packet_(std::move(packet)),
      // No send time indicates packet is not sent immediately, but instead will
      // be put in the pacer queue and later retrieved via
      // GetPacketAndSetSendTime().
      pending_transmission_(!send_time_ms.has_value()),
      storage_type_(storage_type),
      insert_order_(insert_order),
      times_retransmitted_(0) {}

RtpPacketHistory::StoredPacket::StoredPacket(StoredPacket&&) = default;
RtpPacketHistory::StoredPacket& RtpPacketHistory::StoredPacket::operator=(
    RtpPacketHistory::StoredPacket&&) = default;
RtpPacketHistory::StoredPacket::~StoredPacket() = default;

void RtpPacketHistory::StoredPacket::IncrementTimesRetransmitted(
    PacketPrioritySet* priority_set) {
  // Check if this StoredPacket is in the priority set. If so, we need to remove
  // it before updating |times_retransmitted_| since that is used in sorting,
  // and then add it back.
  const bool in_priority_set = priority_set->erase(this) > 0;
  ++times_retransmitted_;
  if (in_priority_set) {
    priority_set->insert(this);
  }
}

bool RtpPacketHistory::MoreUseful::operator()(StoredPacket* lhs,
                                              StoredPacket* rhs) const {
  // Prefer to send packets we haven't already sent as padding.
  if (lhs->times_retransmitted() != rhs->times_retransmitted()) {
    return lhs->times_retransmitted() < rhs->times_retransmitted();
  }
  // All else being equal, prefer newer packets.
  return lhs->insert_order() > rhs->insert_order();
}

RtpPacketHistory::RtpPacketHistory(Clock* clock)
    : clock_(clock),
      number_to_store_(0),
      mode_(StorageMode::kDisabled),
      rtt_ms_(-1),
      retransmittable_packets_inserted_(0) {}

RtpPacketHistory::~RtpPacketHistory() {}

void RtpPacketHistory::SetStorePacketsStatus(StorageMode mode,
                                             size_t number_to_store) {
  RTC_DCHECK_LE(number_to_store, kMaxCapacity);
  rtc::CritScope cs(&lock_);
  if (mode != StorageMode::kDisabled && mode_ != StorageMode::kDisabled) {
    RTC_LOG(LS_WARNING) << "Purging packet history in order to re-set status.";
  }
  Reset();
  mode_ = mode;
  number_to_store_ = std::min(kMaxCapacity, number_to_store);
}

RtpPacketHistory::StorageMode RtpPacketHistory::GetStorageMode() const {
  rtc::CritScope cs(&lock_);
  return mode_;
}

void RtpPacketHistory::SetRtt(int64_t rtt_ms) {
  rtc::CritScope cs(&lock_);
  RTC_DCHECK_GE(rtt_ms, 0);
  rtt_ms_ = rtt_ms;
  // If kStoreAndCull mode is used, packets will be removed after a timeout
  // that depends on the RTT. Changing the RTT may thus cause some packets
  // become "old" and subject to removal.
  CullOldPackets(clock_->TimeInMilliseconds());
}

void RtpPacketHistory::PutRtpPacket(std::unique_ptr<RtpPacketToSend> packet,
                                    StorageType type,
                                    absl::optional<int64_t> send_time_ms) {
  RTC_DCHECK(packet);
  rtc::CritScope cs(&lock_);
  int64_t now_ms = clock_->TimeInMilliseconds();
  if (mode_ == StorageMode::kDisabled) {
    return;
  }

  CullOldPackets(now_ms);

  // Store packet.
  const uint16_t rtp_seq_no = packet->SequenceNumber();
  auto it = packet_history_.emplace(
      rtp_seq_no, StoredPacket(std::move(packet), type, send_time_ms,
                               type != StorageType::kDontRetransmit
                                   ? retransmittable_packets_inserted_++
                                   : 0));
  RTC_DCHECK(it.second);
  StoredPacket& stored_packet = it.first->second;
  if (stored_packet.packet_) {
    // It is an error if this happen. But it can happen if the sequence numbers
    // for some reason restart without that the history has been reset.
    auto size_iterator = packet_size_.find(stored_packet.packet_->size());
    if (size_iterator != packet_size_.end() &&
        size_iterator->second == stored_packet.packet_->SequenceNumber()) {
      packet_size_.erase(size_iterator);
    }
  }

  if (stored_packet.packet_->capture_time_ms() <= 0) {
    stored_packet.packet_->set_capture_time_ms(now_ms);
  }

  if (!start_seqno_) {
    start_seqno_ = rtp_seq_no;
  }

  // Store the sequence number of the last send packet with this size.
  if (type != StorageType::kDontRetransmit) {
    packet_size_[stored_packet.packet_->size()] = rtp_seq_no;
    padding_priority_.insert(&stored_packet);
  }
}

std::unique_ptr<RtpPacketToSend> RtpPacketHistory::GetPacketAndSetSendTime(
    uint16_t sequence_number) {
  rtc::CritScope cs(&lock_);
  if (mode_ == StorageMode::kDisabled) {
    return nullptr;
  }

  int64_t now_ms = clock_->TimeInMilliseconds();
  StoredPacketIterator rtp_it = packet_history_.find(sequence_number);
  if (rtp_it == packet_history_.end()) {
    return nullptr;
  }

  StoredPacket& packet = rtp_it->second;
  if (!VerifyRtt(rtp_it->second, now_ms)) {
    return nullptr;
  }

  if (packet.storage_type() != StorageType::kDontRetransmit &&
      packet.send_time_ms_) {
    packet.IncrementTimesRetransmitted(&padding_priority_);
  }

  // Update send-time and mark as no long in pacer queue.
  packet.send_time_ms_ = now_ms;
  packet.pending_transmission_ = false;

  if (packet.storage_type() == StorageType::kDontRetransmit) {
    // Non retransmittable packet, so call must come from paced sender.
    // Remove from history and return actual packet instance.
    return RemovePacket(rtp_it);
  }

  // Return copy of packet instance since it may need to be retransmitted.
  return absl::make_unique<RtpPacketToSend>(*packet.packet_);
}

absl::optional<RtpPacketHistory::PacketState> RtpPacketHistory::GetPacketState(
    uint16_t sequence_number) const {
  rtc::CritScope cs(&lock_);
  if (mode_ == StorageMode::kDisabled) {
    return absl::nullopt;
  }

  auto rtp_it = packet_history_.find(sequence_number);
  if (rtp_it == packet_history_.end()) {
    return absl::nullopt;
  }

  if (!VerifyRtt(rtp_it->second, clock_->TimeInMilliseconds())) {
    return absl::nullopt;
  }

  return StoredPacketToPacketState(rtp_it->second);
}

bool RtpPacketHistory::VerifyRtt(const RtpPacketHistory::StoredPacket& packet,
                                 int64_t now_ms) const {
  if (packet.send_time_ms_) {
    // Send-time already set, this check must be for a retransmission.
    if (packet.times_retransmitted() > 0 &&
        now_ms < *packet.send_time_ms_ + rtt_ms_) {
      // This packet has already been retransmitted once, and the time since
      // that even is lower than on RTT. Ignore request as this packet is
      // likely already in the network pipe.
      return false;
    }
  }

  return true;
}

std::unique_ptr<RtpPacketToSend> RtpPacketHistory::GetBestFittingPacket(
    size_t packet_length) const {
  rtc::CritScope cs(&lock_);
  if (packet_length < kMinPacketRequestBytes || packet_size_.empty()) {
    return nullptr;
  }

  auto size_iter_upper = packet_size_.upper_bound(packet_length);
  auto size_iter_lower = size_iter_upper;
  if (size_iter_upper == packet_size_.end()) {
    --size_iter_upper;
  }
  if (size_iter_lower != packet_size_.begin()) {
    --size_iter_lower;
  }
  const size_t upper_bound_diff =
      SizeDiff(size_iter_upper->first, packet_length);
  const size_t lower_bound_diff =
      SizeDiff(size_iter_lower->first, packet_length);

  const uint16_t seq_no = upper_bound_diff < lower_bound_diff
                              ? size_iter_upper->second
                              : size_iter_lower->second;
  auto history_it = packet_history_.find(seq_no);
  if (history_it == packet_history_.end()) {
    RTC_LOG(LS_ERROR) << "Can't find packet in history with seq_no" << seq_no;
    RTC_DCHECK(false);
    return nullptr;
  }
  if (!history_it->second.packet_) {
    RTC_LOG(LS_ERROR) << "Packet pointer is null in history for seq_no"
                      << seq_no;
    RTC_DCHECK(false);
    return nullptr;
  }
  RtpPacketToSend* best_packet = history_it->second.packet_.get();
  return absl::make_unique<RtpPacketToSend>(*best_packet);
}

std::unique_ptr<RtpPacketToSend> RtpPacketHistory::GetPayloadPaddingPacket() {
  rtc::CritScope cs(&lock_);
  RTC_DCHECK(mode_ != StorageMode::kDisabled);
  if (padding_priority_.empty()) {
    return nullptr;
  }

  auto best_packet_it = padding_priority_.begin();
  StoredPacket* best_packet = *best_packet_it;
  if (best_packet->pending_transmission_) {
    // Because PacedSender releases it's lock when it calls
    // TimeToSendPadding() there is the potential for a race where a new
    // packet ends up here instead of the regular transmit path. In such a
    // case, just return empty and it will be picked up on the next
    // Process() call.
    return nullptr;
  }

  best_packet->send_time_ms_ = clock_->TimeInMilliseconds();
  best_packet->IncrementTimesRetransmitted(&padding_priority_);

  // Return a copy of the packet.
  return absl::make_unique<RtpPacketToSend>(*best_packet->packet_);
}

void RtpPacketHistory::CullAcknowledgedPackets(
    rtc::ArrayView<const uint16_t> sequence_numbers) {
  rtc::CritScope cs(&lock_);
  if (mode_ == StorageMode::kStoreAndCull) {
    for (uint16_t sequence_number : sequence_numbers) {
      auto stored_packet_it = packet_history_.find(sequence_number);
      if (stored_packet_it != packet_history_.end()) {
        RemovePacket(stored_packet_it);
      }
    }
  }
}

bool RtpPacketHistory::SetPendingTransmission(uint16_t sequence_number) {
  rtc::CritScope cs(&lock_);
  if (mode_ == StorageMode::kDisabled) {
    return false;
  }

  auto rtp_it = packet_history_.find(sequence_number);
  if (rtp_it == packet_history_.end()) {
    return false;
  }

  rtp_it->second.pending_transmission_ = true;
  return true;
}

void RtpPacketHistory::Reset() {
  packet_history_.clear();
  packet_size_.clear();
  padding_priority_.clear();
  start_seqno_.reset();
}

void RtpPacketHistory::CullOldPackets(int64_t now_ms) {
  int64_t packet_duration_ms =
      std::max(kMinPacketDurationRtt * rtt_ms_, kMinPacketDurationMs);
  while (!packet_history_.empty()) {
    auto stored_packet_it = packet_history_.find(*start_seqno_);
    RTC_DCHECK(stored_packet_it != packet_history_.end());

    if (packet_history_.size() >= kMaxCapacity) {
      // We have reached the absolute max capacity, remove one packet
      // unconditionally.
      RemovePacket(stored_packet_it);
      continue;
    }

    const StoredPacket& stored_packet = stored_packet_it->second;
    if (stored_packet_it->second.pending_transmission_) {
      // Don't remove packets in the pacer queue, pending tranmission.
      return;
    }

    if (*stored_packet.send_time_ms_ + packet_duration_ms > now_ms) {
      // Don't cull packets too early to avoid failed retransmission requests.
      return;
    }

    if (packet_history_.size() >= number_to_store_ ||
        (mode_ == StorageMode::kStoreAndCull &&
         *stored_packet.send_time_ms_ +
                 (packet_duration_ms * kPacketCullingDelayFactor) <=
             now_ms)) {
      // Too many packets in history, or this packet has timed out. Remove it
      // and continue.
      RemovePacket(stored_packet_it);
    } else {
      // No more packets can be removed right now.
      return;
    }
  }
}

std::unique_ptr<RtpPacketToSend> RtpPacketHistory::RemovePacket(
    StoredPacketIterator packet_it) {
  // Move the packet out from the StoredPacket container.
  std::unique_ptr<RtpPacketToSend> rtp_packet =
      std::move(packet_it->second.packet_);

  // Check if this is the oldest packet in the history, as this must be updated
  // in order to cull old packets.
  const bool is_first_packet = packet_it->first == start_seqno_;

  // Erase from padding priority set, if eligible.
  if (packet_it->second.storage_type() != StorageType::kDontRetransmit) {
    RTC_CHECK_EQ(padding_priority_.erase(&packet_it->second), 1);
  }

  // Erase the packet from the map, and capture iterator to the next one.
  StoredPacketIterator next_it = packet_history_.erase(packet_it);

  if (is_first_packet) {
    // |next_it| now points to the next element, or to the end. If the end,
    // check if we can wrap around.
    if (next_it == packet_history_.end()) {
      next_it = packet_history_.begin();
    }

    // Update |start_seq_no| to the new oldest item.
    if (next_it != packet_history_.end()) {
      start_seqno_ = next_it->first;
    } else {
      start_seqno_.reset();
    }
  }

  auto size_iterator = packet_size_.find(rtp_packet->size());
  if (size_iterator != packet_size_.end() &&
      size_iterator->second == rtp_packet->SequenceNumber()) {
    packet_size_.erase(size_iterator);
  }

  return rtp_packet;
}

RtpPacketHistory::PacketState RtpPacketHistory::StoredPacketToPacketState(
    const RtpPacketHistory::StoredPacket& stored_packet) {
  RtpPacketHistory::PacketState state;
  state.rtp_sequence_number = stored_packet.packet_->SequenceNumber();
  state.send_time_ms = stored_packet.send_time_ms_;
  state.capture_time_ms = stored_packet.packet_->capture_time_ms();
  state.ssrc = stored_packet.packet_->Ssrc();
  state.packet_size = stored_packet.packet_->size();
  state.times_retransmitted = stored_packet.times_retransmitted();
  state.pending_transmission = stored_packet.pending_transmission_;
  return state;
}

}  // namespace webrtc
