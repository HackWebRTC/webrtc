/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/pacing/include/paced_sender.h"

#include <assert.h>

#include <map>
#include <set>

#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

namespace {
// Time limit in milliseconds between packet bursts.
const int kMinPacketLimitMs = 5;

// Upper cap on process interval, in case process has not been called in a long
// time.
const int kMaxIntervalTimeMs = 30;

// Max time that the first packet in the queue can sit in the queue if no
// packets are sent, regardless of buffer state. In practice only in effect at
// low bitrates (less than 320 kbits/s).
const int kMaxQueueTimeWithoutSendingUs = 30000;

}  // namespace

namespace webrtc {
namespace paced_sender {
struct Packet {
  Packet(uint32_t ssrc,
         uint16_t seq_number,
         int64_t capture_time_ms,
         int64_t enqueue_time_ms,
         int length_in_bytes,
         bool retransmission)
      : ssrc(ssrc),
        sequence_number(seq_number),
        capture_time_ms(capture_time_ms),
        enqueue_time_ms(enqueue_time_ms),
        bytes(length_in_bytes),
        retransmission(retransmission) {}
  uint32_t ssrc;
  uint16_t sequence_number;
  int64_t capture_time_ms;
  int64_t enqueue_time_ms;
  int bytes;
  bool retransmission;
};

// STL list style class which prevents duplicates in the list.
class PacketList {
 public:
  PacketList() {};

  bool empty() const {
    return packet_list_.empty();
  }

  Packet front() const {
    return packet_list_.front();
  }

  void pop_front() {
    Packet& packet = packet_list_.front();
    uint16_t sequence_number = packet.sequence_number;
    uint32_t ssrc = packet.ssrc;
    packet_list_.pop_front();
    sequence_number_set_[ssrc].erase(sequence_number);
  }

  void push_back(const Packet& packet) {
    if (sequence_number_set_[packet.ssrc].find(packet.sequence_number) ==
        sequence_number_set_[packet.ssrc].end()) {
      // Don't insert duplicates.
      packet_list_.push_back(packet);
      sequence_number_set_[packet.ssrc].insert(packet.sequence_number);
    }
  }

 private:
  std::list<Packet> packet_list_;
  std::map<uint32_t, std::set<uint16_t> > sequence_number_set_;
};

class IntervalBudget {
 public:
  explicit IntervalBudget(int initial_target_rate_kbps)
      : target_rate_kbps_(initial_target_rate_kbps),
        bytes_remaining_(0) {}

  void set_target_rate_kbps(int target_rate_kbps) {
    target_rate_kbps_ = target_rate_kbps;
  }

  void IncreaseBudget(int delta_time_ms) {
    int bytes = target_rate_kbps_ * delta_time_ms / 8;
    if (bytes_remaining_ < 0) {
      // We overused last interval, compensate this interval.
      bytes_remaining_ = bytes_remaining_ + bytes;
    } else {
      // If we underused last interval we can't use it this interval.
      bytes_remaining_ = bytes;
    }
  }

  void UseBudget(int bytes) {
    bytes_remaining_ = std::max(bytes_remaining_ - bytes,
                                -500 * target_rate_kbps_ / 8);
  }

  int bytes_remaining() const { return bytes_remaining_; }

 private:
  int target_rate_kbps_;
  int bytes_remaining_;
};
}  // namespace paced_sender

const float PacedSender::kDefaultPaceMultiplier = 2.5f;

PacedSender::PacedSender(Clock* clock,
                         Callback* callback,
                         int max_bitrate_kbps,
                         int min_bitrate_kbps)
    : clock_(clock),
      callback_(callback),
      critsect_(CriticalSectionWrapper::CreateCriticalSection()),
      enabled_(true),
      paused_(false),
      max_queue_length_ms_(kDefaultMaxQueueLengthMs),
      media_budget_(new paced_sender::IntervalBudget(max_bitrate_kbps)),
      padding_budget_(new paced_sender::IntervalBudget(min_bitrate_kbps)),
      time_last_update_us_(clock->TimeInMicroseconds()),
      capture_time_ms_last_queued_(0),
      capture_time_ms_last_sent_(0),
      high_priority_packets_(new paced_sender::PacketList),
      normal_priority_packets_(new paced_sender::PacketList),
      low_priority_packets_(new paced_sender::PacketList) {
  UpdateBytesPerInterval(kMinPacketLimitMs);
}

PacedSender::~PacedSender() {}

void PacedSender::Pause() {
  CriticalSectionScoped cs(critsect_.get());
  paused_ = true;
}

void PacedSender::Resume() {
  CriticalSectionScoped cs(critsect_.get());
  paused_ = false;
}

void PacedSender::SetStatus(bool enable) {
  CriticalSectionScoped cs(critsect_.get());
  enabled_ = enable;
}

bool PacedSender::Enabled() const {
  CriticalSectionScoped cs(critsect_.get());
  return enabled_;
}

void PacedSender::UpdateBitrate(int max_bitrate_kbps,
                                int min_bitrate_kbps) {
  CriticalSectionScoped cs(critsect_.get());
  media_budget_->set_target_rate_kbps(max_bitrate_kbps);
  padding_budget_->set_target_rate_kbps(min_bitrate_kbps);
}

bool PacedSender::SendPacket(Priority priority, uint32_t ssrc,
    uint16_t sequence_number, int64_t capture_time_ms, int bytes,
    bool retransmission) {
  CriticalSectionScoped cs(critsect_.get());

  if (!enabled_) {
    return true;  // We can send now.
  }
  if (capture_time_ms < 0) {
    capture_time_ms = clock_->TimeInMilliseconds();
  }
  if (priority != kHighPriority &&
      capture_time_ms > capture_time_ms_last_queued_) {
    capture_time_ms_last_queued_ = capture_time_ms;
    TRACE_EVENT_ASYNC_BEGIN1("webrtc_rtp", "PacedSend", capture_time_ms,
                             "capture_time_ms", capture_time_ms);
  }
  paced_sender::PacketList* packet_list = NULL;
  switch (priority) {
    case kHighPriority:
      packet_list = high_priority_packets_.get();
      break;
    case kNormalPriority:
      packet_list = normal_priority_packets_.get();
      break;
    case kLowPriority:
      packet_list = low_priority_packets_.get();
      break;
  }
  packet_list->push_back(paced_sender::Packet(ssrc,
                                              sequence_number,
                                              capture_time_ms,
                                              clock_->TimeInMilliseconds(),
                                              bytes,
                                              retransmission));
  return false;
}

void PacedSender::set_max_queue_length_ms(int max_queue_length_ms) {
  CriticalSectionScoped cs(critsect_.get());
  max_queue_length_ms_ = max_queue_length_ms;
}

int PacedSender::QueueInMs() const {
  CriticalSectionScoped cs(critsect_.get());
  int64_t now_ms = clock_->TimeInMilliseconds();
  int64_t oldest_packet_enqueue_time = now_ms;
  if (!high_priority_packets_->empty()) {
    oldest_packet_enqueue_time =
        std::min(oldest_packet_enqueue_time,
                 high_priority_packets_->front().enqueue_time_ms);
  }
  if (!normal_priority_packets_->empty()) {
    oldest_packet_enqueue_time =
        std::min(oldest_packet_enqueue_time,
                 normal_priority_packets_->front().enqueue_time_ms);
  }
  if (!low_priority_packets_->empty()) {
    oldest_packet_enqueue_time =
        std::min(oldest_packet_enqueue_time,
                 low_priority_packets_->front().enqueue_time_ms);
  }
  return now_ms - oldest_packet_enqueue_time;
}

int32_t PacedSender::TimeUntilNextProcess() {
  CriticalSectionScoped cs(critsect_.get());
  int64_t elapsed_time_ms = (clock_->TimeInMicroseconds() -
      time_last_update_us_ + 500) / 1000;
  if (elapsed_time_ms <= 0) {
    return kMinPacketLimitMs;
  }
  if (elapsed_time_ms >= kMinPacketLimitMs) {
    return 0;
  }
  return kMinPacketLimitMs - elapsed_time_ms;
}

int32_t PacedSender::Process() {
  int64_t now_us = clock_->TimeInMicroseconds();
  CriticalSectionScoped cs(critsect_.get());
  int elapsed_time_ms = (now_us - time_last_update_us_ + 500) / 1000;
  time_last_update_us_ = now_us;
  if (!enabled_) {
    return 0;
  }
  if (!paused_) {
    if (elapsed_time_ms > 0) {
      uint32_t delta_time_ms = std::min(kMaxIntervalTimeMs, elapsed_time_ms);
      UpdateBytesPerInterval(delta_time_ms);
    }
    paced_sender::PacketList* packet_list;
    while (ShouldSendNextPacket(&packet_list)) {
      if (!SendPacketFromList(packet_list))
        return 0;
    }
    if (high_priority_packets_->empty() &&
        normal_priority_packets_->empty() &&
        low_priority_packets_->empty() &&
        padding_budget_->bytes_remaining() > 0) {
      int padding_needed = padding_budget_->bytes_remaining();
      critsect_->Leave();
      int bytes_sent = callback_->TimeToSendPadding(padding_needed);
      critsect_->Enter();
      media_budget_->UseBudget(bytes_sent);
      padding_budget_->UseBudget(bytes_sent);
    }
  }
  return 0;
}

bool PacedSender::SendPacketFromList(paced_sender::PacketList* packet_list)
    EXCLUSIVE_LOCKS_REQUIRED(critsect_.get()) {
  paced_sender::Packet packet = GetNextPacketFromList(packet_list);
  critsect_->Leave();

  const bool success = callback_->TimeToSendPacket(packet.ssrc,
                                                   packet.sequence_number,
                                                   packet.capture_time_ms,
                                                   packet.retransmission);
  critsect_->Enter();
  // If packet cannot be sent then keep it in packet list and exit early.
  // There's no need to send more packets.
  if (!success) {
    return false;
  }
  packet_list->pop_front();
  const bool last_packet =
      packet_list->empty() ||
      packet_list->front().capture_time_ms > packet.capture_time_ms;
  if (packet_list != high_priority_packets_.get()) {
    if (packet.capture_time_ms > capture_time_ms_last_sent_) {
      capture_time_ms_last_sent_ = packet.capture_time_ms;
    } else if (packet.capture_time_ms == capture_time_ms_last_sent_ &&
               last_packet) {
      TRACE_EVENT_ASYNC_END0("webrtc_rtp", "PacedSend", packet.capture_time_ms);
    }
  }
  return true;
}

void PacedSender::UpdateBytesPerInterval(uint32_t delta_time_ms) {
  media_budget_->IncreaseBudget(delta_time_ms);
  padding_budget_->IncreaseBudget(delta_time_ms);
}

bool PacedSender::ShouldSendNextPacket(paced_sender::PacketList** packet_list) {
  *packet_list = NULL;
  if (media_budget_->bytes_remaining() <= 0) {
    // All bytes consumed for this interval.
    // Check if we have not sent in a too long time.
    if (clock_->TimeInMicroseconds() - time_last_send_us_ >
        kMaxQueueTimeWithoutSendingUs) {
      if (!high_priority_packets_->empty()) {
        *packet_list = high_priority_packets_.get();
        return true;
      }
      if (!normal_priority_packets_->empty()) {
        *packet_list = normal_priority_packets_.get();
        return true;
      }
    }
    // Send any old packets to avoid queuing for too long.
    if (max_queue_length_ms_ >= 0 && QueueInMs() > max_queue_length_ms_) {
      int64_t high_priority_capture_time = -1;
      if (!high_priority_packets_->empty()) {
        high_priority_capture_time =
            high_priority_packets_->front().capture_time_ms;
        *packet_list = high_priority_packets_.get();
      }
      if (!normal_priority_packets_->empty() &&
          (high_priority_capture_time == -1 ||
           high_priority_capture_time >
               normal_priority_packets_->front().capture_time_ms)) {
        *packet_list = normal_priority_packets_.get();
      }
      if (*packet_list)
        return true;
    }
    return false;
  }
  if (!high_priority_packets_->empty()) {
    *packet_list = high_priority_packets_.get();
    return true;
  }
  if (!normal_priority_packets_->empty()) {
    *packet_list = normal_priority_packets_.get();
    return true;
  }
  if (!low_priority_packets_->empty()) {
    *packet_list = low_priority_packets_.get();
    return true;
  }
  return false;
}

paced_sender::Packet PacedSender::GetNextPacketFromList(
    paced_sender::PacketList* packets) {
  paced_sender::Packet packet = packets->front();
  UpdateMediaBytesSent(packet.bytes);
  return packet;
}

void PacedSender::UpdateMediaBytesSent(int num_bytes) {
  time_last_send_us_ = clock_->TimeInMicroseconds();
  media_budget_->UseBudget(num_bytes);
  padding_budget_->UseBudget(num_bytes);
}

}  // namespace webrtc
