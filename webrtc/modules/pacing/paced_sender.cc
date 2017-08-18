/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/pacing/paced_sender.h"

#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <vector>

#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/pacing/alr_detector.h"
#include "webrtc/modules/pacing/bitrate_prober.h"
#include "webrtc/modules/pacing/interval_budget.h"
#include "webrtc/modules/utility/include/process_thread.h"
#include "webrtc/rtc_base/checks.h"
#include "webrtc/rtc_base/logging.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/system_wrappers/include/field_trial.h"

namespace {
// Time limit in milliseconds between packet bursts.
const int64_t kMinPacketLimitMs = 5;
const int64_t kPausedPacketIntervalMs = 500;

// Upper cap on process interval, in case process has not been called in a long
// time.
const int64_t kMaxIntervalTimeMs = 30;

}  // namespace

// TODO(sprang): Move at least PacketQueue out to separate
// files, so that we can more easily test them.

namespace webrtc {
namespace paced_sender {
struct Packet {
  Packet(RtpPacketSender::Priority priority,
         uint32_t ssrc,
         uint16_t seq_number,
         int64_t capture_time_ms,
         int64_t enqueue_time_ms,
         size_t length_in_bytes,
         bool retransmission,
         uint64_t enqueue_order)
      : priority(priority),
        ssrc(ssrc),
        sequence_number(seq_number),
        capture_time_ms(capture_time_ms),
        enqueue_time_ms(enqueue_time_ms),
        sum_paused_ms(0),
        bytes(length_in_bytes),
        retransmission(retransmission),
        enqueue_order(enqueue_order) {}

  RtpPacketSender::Priority priority;
  uint32_t ssrc;
  uint16_t sequence_number;
  int64_t capture_time_ms;  // Absolute time of frame capture.
  int64_t enqueue_time_ms;  // Absolute time of pacer queue entry.
  int64_t sum_paused_ms;    // Sum of time spent in queue while pacer is paused.
  size_t bytes;
  bool retransmission;
  uint64_t enqueue_order;
  std::list<Packet>::iterator this_it;
};

// Used by priority queue to sort packets.
struct Comparator {
  bool operator()(const Packet* first, const Packet* second) {
    // Highest prio = 0.
    if (first->priority != second->priority)
      return first->priority > second->priority;

    // Retransmissions go first.
    if (second->retransmission != first->retransmission)
      return second->retransmission;

    // Older frames have higher prio.
    if (first->capture_time_ms != second->capture_time_ms)
      return first->capture_time_ms > second->capture_time_ms;

    return first->enqueue_order > second->enqueue_order;
  }
};

// Class encapsulating a priority queue with some extensions.
class PacketQueue {
 public:
  explicit PacketQueue(const Clock* clock)
      : bytes_(0),
        clock_(clock),
        queue_time_sum_(0),
        time_last_updated_(clock_->TimeInMilliseconds()),
        paused_(false) {}
  virtual ~PacketQueue() {}

  void Push(const Packet& packet) {
    if (!AddToDupeSet(packet))
      return;

    UpdateQueueTime(packet.enqueue_time_ms);

    // Store packet in list, use pointers in priority queue for cheaper moves.
    // Packets have a handle to its own iterator in the list, for easy removal
    // when popping from queue.
    packet_list_.push_front(packet);
    std::list<Packet>::iterator it = packet_list_.begin();
    it->this_it = it;          // Handle for direct removal from list.
    prio_queue_.push(&(*it));  // Pointer into list.
    bytes_ += packet.bytes;
  }

  const Packet& BeginPop() {
    const Packet& packet = *prio_queue_.top();
    prio_queue_.pop();
    return packet;
  }

  void CancelPop(const Packet& packet) { prio_queue_.push(&(*packet.this_it)); }

  void FinalizePop(const Packet& packet) {
    RemoveFromDupeSet(packet);
    bytes_ -= packet.bytes;
    int64_t packet_queue_time_ms = time_last_updated_ - packet.enqueue_time_ms;
    RTC_DCHECK_LE(packet.sum_paused_ms, packet_queue_time_ms);
    packet_queue_time_ms -= packet.sum_paused_ms;
    RTC_DCHECK_LE(packet_queue_time_ms, queue_time_sum_);
    queue_time_sum_ -= packet_queue_time_ms;
    packet_list_.erase(packet.this_it);
    RTC_DCHECK_EQ(packet_list_.size(), prio_queue_.size());
    if (packet_list_.empty())
      RTC_DCHECK_EQ(0, queue_time_sum_);
  }

  bool Empty() const { return prio_queue_.empty(); }

  size_t SizeInPackets() const { return prio_queue_.size(); }

  uint64_t SizeInBytes() const { return bytes_; }

  int64_t OldestEnqueueTimeMs() const {
    auto it = packet_list_.rbegin();
    if (it == packet_list_.rend())
      return 0;
    return it->enqueue_time_ms;
  }

  void UpdateQueueTime(int64_t timestamp_ms) {
    RTC_DCHECK_GE(timestamp_ms, time_last_updated_);
    if (timestamp_ms == time_last_updated_)
      return;

    int64_t delta_ms = timestamp_ms - time_last_updated_;

    if (paused_) {
      // Increase per-packet accumulators of time spent in queue while paused,
      // so that we can disregard that when subtracting main accumulator when
      // popping packet from the queue.
      for (auto& it : packet_list_) {
        it.sum_paused_ms += delta_ms;
      }
    } else {
      // Use packet packet_list_.size() not prio_queue_.size() here, as there
      // might be an outstanding element popped from prio_queue_ currently in
      // the SendPacket() call, while packet_list_ will always be correct.
      queue_time_sum_ += delta_ms * packet_list_.size();
    }
    time_last_updated_ = timestamp_ms;
  }

  void SetPauseState(bool paused, int64_t timestamp_ms) {
    if (paused_ == paused)
      return;
    UpdateQueueTime(timestamp_ms);
    paused_ = paused;
  }

  int64_t AverageQueueTimeMs() const {
    if (prio_queue_.empty())
      return 0;
    return queue_time_sum_ / packet_list_.size();
  }

 private:
  // Try to add a packet to the set of ssrc/seqno identifiers currently in the
  // queue. Return true if inserted, false if this is a duplicate.
  bool AddToDupeSet(const Packet& packet) {
    SsrcSeqNoMap::iterator it = dupe_map_.find(packet.ssrc);
    if (it == dupe_map_.end()) {
      // First for this ssrc, just insert.
      dupe_map_[packet.ssrc].insert(packet.sequence_number);
      return true;
    }

    // Insert returns a pair, where second is a bool set to true if new element.
    return it->second.insert(packet.sequence_number).second;
  }

  void RemoveFromDupeSet(const Packet& packet) {
    SsrcSeqNoMap::iterator it = dupe_map_.find(packet.ssrc);
    RTC_DCHECK(it != dupe_map_.end());
    it->second.erase(packet.sequence_number);
    if (it->second.empty()) {
      dupe_map_.erase(it);
    }
  }

  // List of packets, in the order the were enqueued. Since dequeueing may
  // occur out of order, use list instead of vector.
  std::list<Packet> packet_list_;
  // Priority queue of the packets, sorted according to Comparator.
  // Use pointers into list, to avoid moving whole struct within heap.
  std::priority_queue<Packet*, std::vector<Packet*>, Comparator> prio_queue_;
  // Total number of bytes in the queue.
  uint64_t bytes_;
  // Map<ssrc, std::set<seq_no> >, for checking duplicates.
  typedef std::map<uint32_t, std::set<uint16_t> > SsrcSeqNoMap;
  SsrcSeqNoMap dupe_map_;
  const Clock* const clock_;
  int64_t queue_time_sum_;
  int64_t time_last_updated_;
  bool paused_;
};

}  // namespace paced_sender

const int64_t PacedSender::kMaxQueueLengthMs = 2000;
const float PacedSender::kDefaultPaceMultiplier = 2.5f;

PacedSender::PacedSender(const Clock* clock,
                         PacketSender* packet_sender,
                         RtcEventLog* event_log)
    : clock_(clock),
      packet_sender_(packet_sender),
      alr_detector_(new AlrDetector()),
      paused_(false),
      media_budget_(new IntervalBudget(0)),
      padding_budget_(new IntervalBudget(0)),
      prober_(new BitrateProber(event_log)),
      probing_send_failure_(false),
      estimated_bitrate_bps_(0),
      min_send_bitrate_kbps_(0u),
      max_padding_bitrate_kbps_(0u),
      pacing_bitrate_kbps_(0),
      time_last_update_us_(clock->TimeInMicroseconds()),
      first_sent_packet_ms_(-1),
      packets_(new paced_sender::PacketQueue(clock)),
      packet_counter_(0),
      pacing_factor_(kDefaultPaceMultiplier),
      queue_time_limit(kMaxQueueLengthMs) {
  UpdateBudgetWithElapsedTime(kMinPacketLimitMs);
}

PacedSender::~PacedSender() {}

void PacedSender::CreateProbeCluster(int bitrate_bps) {
  rtc::CritScope cs(&critsect_);
  prober_->CreateProbeCluster(bitrate_bps, clock_->TimeInMilliseconds());
}

void PacedSender::Pause() {
  {
    rtc::CritScope cs(&critsect_);
    if (!paused_)
      LOG(LS_INFO) << "PacedSender paused.";
    paused_ = true;
    packets_->SetPauseState(true, clock_->TimeInMilliseconds());
  }
  // Tell the process thread to call our TimeUntilNextProcess() method to get
  // a new (longer) estimate for when to call Process().
  if (process_thread_)
    process_thread_->WakeUp(this);
}

void PacedSender::Resume() {
  {
    rtc::CritScope cs(&critsect_);
    if (paused_)
      LOG(LS_INFO) << "PacedSender resumed.";
    paused_ = false;
    packets_->SetPauseState(false, clock_->TimeInMilliseconds());
  }
  // Tell the process thread to call our TimeUntilNextProcess() method to
  // refresh the estimate for when to call Process().
  if (process_thread_)
    process_thread_->WakeUp(this);
}

void PacedSender::SetProbingEnabled(bool enabled) {
  RTC_CHECK_EQ(0, packet_counter_);
  rtc::CritScope cs(&critsect_);
  prober_->SetEnabled(enabled);
}

void PacedSender::SetEstimatedBitrate(uint32_t bitrate_bps) {
  if (bitrate_bps == 0)
    LOG(LS_ERROR) << "PacedSender is not designed to handle 0 bitrate.";
  rtc::CritScope cs(&critsect_);
  estimated_bitrate_bps_ = bitrate_bps;
  padding_budget_->set_target_rate_kbps(
      std::min(estimated_bitrate_bps_ / 1000, max_padding_bitrate_kbps_));
  pacing_bitrate_kbps_ =
      std::max(min_send_bitrate_kbps_, estimated_bitrate_bps_ / 1000) *
      pacing_factor_;
  alr_detector_->SetEstimatedBitrate(bitrate_bps);
}

void PacedSender::SetSendBitrateLimits(int min_send_bitrate_bps,
                                       int padding_bitrate) {
  rtc::CritScope cs(&critsect_);
  min_send_bitrate_kbps_ = min_send_bitrate_bps / 1000;
  pacing_bitrate_kbps_ =
      std::max(min_send_bitrate_kbps_, estimated_bitrate_bps_ / 1000) *
      pacing_factor_;
  max_padding_bitrate_kbps_ = padding_bitrate / 1000;
  padding_budget_->set_target_rate_kbps(
      std::min(estimated_bitrate_bps_ / 1000, max_padding_bitrate_kbps_));
}

void PacedSender::InsertPacket(RtpPacketSender::Priority priority,
                               uint32_t ssrc,
                               uint16_t sequence_number,
                               int64_t capture_time_ms,
                               size_t bytes,
                               bool retransmission) {
  rtc::CritScope cs(&critsect_);
  RTC_DCHECK(estimated_bitrate_bps_ > 0)
        << "SetEstimatedBitrate must be called before InsertPacket.";

  int64_t now_ms = clock_->TimeInMilliseconds();
  prober_->OnIncomingPacket(bytes);

  if (capture_time_ms < 0)
    capture_time_ms = now_ms;

  packets_->Push(paced_sender::Packet(priority, ssrc, sequence_number,
                                      capture_time_ms, now_ms, bytes,
                                      retransmission, packet_counter_++));
}

int64_t PacedSender::ExpectedQueueTimeMs() const {
  rtc::CritScope cs(&critsect_);
  RTC_DCHECK_GT(pacing_bitrate_kbps_, 0);
  return static_cast<int64_t>(packets_->SizeInBytes() * 8 /
                              pacing_bitrate_kbps_);
}

rtc::Optional<int64_t> PacedSender::GetApplicationLimitedRegionStartTime()
    const {
  rtc::CritScope cs(&critsect_);
  return alr_detector_->GetApplicationLimitedRegionStartTime();
}

size_t PacedSender::QueueSizePackets() const {
  rtc::CritScope cs(&critsect_);
  return packets_->SizeInPackets();
}

int64_t PacedSender::FirstSentPacketTimeMs() const {
  rtc::CritScope cs(&critsect_);
  return first_sent_packet_ms_;
}

int64_t PacedSender::QueueInMs() const {
  rtc::CritScope cs(&critsect_);

  int64_t oldest_packet = packets_->OldestEnqueueTimeMs();
  if (oldest_packet == 0)
    return 0;

  return clock_->TimeInMilliseconds() - oldest_packet;
}

int64_t PacedSender::AverageQueueTimeMs() {
  rtc::CritScope cs(&critsect_);
  packets_->UpdateQueueTime(clock_->TimeInMilliseconds());
  return packets_->AverageQueueTimeMs();
}

int64_t PacedSender::TimeUntilNextProcess() {
  rtc::CritScope cs(&critsect_);
  int64_t elapsed_time_us = clock_->TimeInMicroseconds() - time_last_update_us_;
  int64_t elapsed_time_ms = (elapsed_time_us + 500) / 1000;
  // When paused we wake up every 500 ms to send a padding packet to ensure
  // we won't get stuck in the paused state due to no feedback being received.
  if (paused_)
    return std::max<int64_t>(kPausedPacketIntervalMs - elapsed_time_ms, 0);

  if (prober_->IsProbing()) {
    int64_t ret = prober_->TimeUntilNextProbe(clock_->TimeInMilliseconds());
    if (ret > 0 || (ret == 0 && !probing_send_failure_))
      return ret;
  }
  return std::max<int64_t>(kMinPacketLimitMs - elapsed_time_ms, 0);
}

void PacedSender::Process() {
  int64_t now_us = clock_->TimeInMicroseconds();
  rtc::CritScope cs(&critsect_);
  int64_t elapsed_time_ms = std::min(
      kMaxIntervalTimeMs, (now_us - time_last_update_us_ + 500) / 1000);
  int target_bitrate_kbps = pacing_bitrate_kbps_;

  if (paused_) {
    PacedPacketInfo pacing_info;
    time_last_update_us_ = now_us;
    // We can not send padding unless a normal packet has first been sent. If we
    // do, timestamps get messed up.
    if (packet_counter_ == 0)
      return;
    size_t bytes_sent = SendPadding(1, pacing_info);
    alr_detector_->OnBytesSent(bytes_sent, elapsed_time_ms);
    return;
  }

  if (elapsed_time_ms > 0) {
    size_t queue_size_bytes = packets_->SizeInBytes();
    if (queue_size_bytes > 0) {
      // Assuming equal size packets and input/output rate, the average packet
      // has avg_time_left_ms left to get queue_size_bytes out of the queue, if
      // time constraint shall be met. Determine bitrate needed for that.
      packets_->UpdateQueueTime(clock_->TimeInMilliseconds());
      int64_t avg_time_left_ms = std::max<int64_t>(
          1, queue_time_limit - packets_->AverageQueueTimeMs());
      int min_bitrate_needed_kbps =
          static_cast<int>(queue_size_bytes * 8 / avg_time_left_ms);
      if (min_bitrate_needed_kbps > target_bitrate_kbps)
        target_bitrate_kbps = min_bitrate_needed_kbps;
    }

    media_budget_->set_target_rate_kbps(target_bitrate_kbps);
    UpdateBudgetWithElapsedTime(elapsed_time_ms);
  }

  time_last_update_us_ = now_us;

  bool is_probing = prober_->IsProbing();
  PacedPacketInfo pacing_info;
  size_t bytes_sent = 0;
  size_t recommended_probe_size = 0;
  if (is_probing) {
    pacing_info = prober_->CurrentCluster();
    recommended_probe_size = prober_->RecommendedMinProbeSize();
  }
  while (!packets_->Empty()) {
    // Since we need to release the lock in order to send, we first pop the
    // element from the priority queue but keep it in storage, so that we can
    // reinsert it if send fails.
    const paced_sender::Packet& packet = packets_->BeginPop();

    if (SendPacket(packet, pacing_info)) {
      // Send succeeded, remove it from the queue.
      if (first_sent_packet_ms_ == -1)
        first_sent_packet_ms_ = clock_->TimeInMilliseconds();
      bytes_sent += packet.bytes;
      packets_->FinalizePop(packet);
      if (is_probing && bytes_sent > recommended_probe_size)
        break;
    } else {
      // Send failed, put it back into the queue.
      packets_->CancelPop(packet);
      break;
    }
  }

  if (packets_->Empty()) {
    // We can not send padding unless a normal packet has first been sent. If we
    // do, timestamps get messed up.
    if (packet_counter_ > 0) {
      int padding_needed =
          static_cast<int>(is_probing ? (recommended_probe_size - bytes_sent)
                                      : padding_budget_->bytes_remaining());
      if (padding_needed > 0)
        bytes_sent += SendPadding(padding_needed, pacing_info);
    }
  }
  if (is_probing) {
    probing_send_failure_ = bytes_sent == 0;
    if (!probing_send_failure_)
      prober_->ProbeSent(clock_->TimeInMilliseconds(), bytes_sent);
  }
  alr_detector_->OnBytesSent(bytes_sent, elapsed_time_ms);
}

void PacedSender::ProcessThreadAttached(ProcessThread* process_thread) {
  LOG(LS_INFO) << "ProcessThreadAttached 0x" << std::hex << process_thread;
  process_thread_ = process_thread;
}

bool PacedSender::SendPacket(const paced_sender::Packet& packet,
                             const PacedPacketInfo& pacing_info) {
  RTC_DCHECK(!paused_);
  if (media_budget_->bytes_remaining() == 0 &&
      pacing_info.probe_cluster_id == PacedPacketInfo::kNotAProbe) {
    return false;
  }

  critsect_.Leave();
  const bool success = packet_sender_->TimeToSendPacket(
      packet.ssrc, packet.sequence_number, packet.capture_time_ms,
      packet.retransmission, pacing_info);
  critsect_.Enter();

  if (success) {
    // TODO(holmer): High priority packets should only be accounted for if we
    // are allocating bandwidth for audio.
    if (packet.priority != kHighPriority) {
      // Update media bytes sent.
      // TODO(eladalon): TimeToSendPacket() can also return |true| in some
      // situations where nothing actually ended up being sent to the network,
      // and we probably don't want to update the budget in such cases.
      // https://bugs.chromium.org/p/webrtc/issues/detail?id=8052
      UpdateBudgetWithBytesSent(packet.bytes);
    }
  }

  return success;
}

size_t PacedSender::SendPadding(size_t padding_needed,
                                const PacedPacketInfo& pacing_info) {
  RTC_DCHECK_GT(packet_counter_, 0);
  critsect_.Leave();
  size_t bytes_sent =
      packet_sender_->TimeToSendPadding(padding_needed, pacing_info);
  critsect_.Enter();

  if (bytes_sent > 0) {
    UpdateBudgetWithBytesSent(bytes_sent);
  }
  return bytes_sent;
}

void PacedSender::UpdateBudgetWithElapsedTime(int64_t delta_time_ms) {
  media_budget_->IncreaseBudget(delta_time_ms);
  padding_budget_->IncreaseBudget(delta_time_ms);
}

void PacedSender::UpdateBudgetWithBytesSent(size_t bytes_sent) {
  media_budget_->UseBudget(bytes_sent);
  padding_budget_->UseBudget(bytes_sent);
}

void PacedSender::SetPacingFactor(float pacing_factor) {
  rtc::CritScope cs(&critsect_);
  pacing_factor_ = pacing_factor;
}

void PacedSender::SetQueueTimeLimit(int limit_ms) {
  rtc::CritScope cs(&critsect_);
  queue_time_limit = limit_ms;
}

}  // namespace webrtc
