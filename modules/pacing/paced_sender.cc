/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/paced_sender.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "modules/pacing/bitrate_prober.h"
#include "modules/pacing/interval_budget.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace {
// Time limit in milliseconds between packet bursts.
constexpr TimeDelta kDefaultMinPacketLimit = TimeDelta::Millis<5>();
constexpr TimeDelta kCongestedPacketInterval = TimeDelta::Millis<500>();
constexpr TimeDelta kPausedProcessInterval = kCongestedPacketInterval;
constexpr TimeDelta kMaxElapsedTime = TimeDelta::Seconds<2>();

// Upper cap on process interval, in case process has not been called in a long
// time.
constexpr TimeDelta kMaxProcessingInterval = TimeDelta::Millis<30>();

bool IsDisabled(const WebRtcKeyValueConfig& field_trials,
                absl::string_view key) {
  return field_trials.Lookup(key).find("Disabled") == 0;
}

bool IsEnabled(const WebRtcKeyValueConfig& field_trials,
               absl::string_view key) {
  return field_trials.Lookup(key).find("Enabled") == 0;
}

int GetPriorityForType(RtpPacketToSend::Type type) {
  switch (type) {
    case RtpPacketToSend::Type::kAudio:
      // Audio is always prioritized over other packet types.
      return 0;
    case RtpPacketToSend::Type::kRetransmission:
      // Send retransmissions before new media.
      return 1;
    case RtpPacketToSend::Type::kVideo:
      // Video has "normal" priority, in the old speak.
      return 2;
    case RtpPacketToSend::Type::kForwardErrorCorrection:
      // Send redundancy concurrently to video. If it is delayed it might have a
      // lower chance of being useful.
      return 2;
    case RtpPacketToSend::Type::kPadding:
      // Packets that are in themselves likely useless, only sent to keep the
      // BWE high.
      return 3;
  }
}

}  // namespace
const int64_t PacedSender::kMaxQueueLengthMs = 2000;
const float PacedSender::kDefaultPaceMultiplier = 2.5f;

PacedSender::PacedSender(Clock* clock,
                         PacketRouter* packet_router,
                         RtcEventLog* event_log,
                         const WebRtcKeyValueConfig* field_trials)
    : clock_(clock),
      packet_router_(packet_router),
      fallback_field_trials_(
          !field_trials ? absl::make_unique<FieldTrialBasedConfig>() : nullptr),
      field_trials_(field_trials ? field_trials : fallback_field_trials_.get()),
      drain_large_queues_(
          !IsDisabled(*field_trials_, "WebRTC-Pacer-DrainQueue")),
      send_padding_if_silent_(
          IsEnabled(*field_trials_, "WebRTC-Pacer-PadInSilence")),
      pace_audio_(!IsDisabled(*field_trials_, "WebRTC-Pacer-BlockAudio")),
      min_packet_limit_(kDefaultMinPacketLimit),
      last_timestamp_(clock_->CurrentTime()),
      paused_(false),
      media_budget_(0),
      padding_budget_(0),
      prober_(*field_trials_),
      probing_send_failure_(false),
      pacing_bitrate_(DataRate::Zero()),
      time_last_process_(clock->CurrentTime()),
      last_send_time_(time_last_process_),
      packets_(time_last_process_, field_trials),
      packet_counter_(0),
      congestion_window_size_(DataSize::PlusInfinity()),
      outstanding_data_(DataSize::Zero()),
      process_thread_(nullptr),
      queue_time_limit(TimeDelta::ms(kMaxQueueLengthMs)),
      account_for_audio_(false),
      legacy_packet_referencing_(
          IsEnabled(*field_trials_, "WebRTC-Pacer-LegacyPacketReferencing")) {
  if (!drain_large_queues_) {
    RTC_LOG(LS_WARNING) << "Pacer queues will not be drained,"
                           "pushback experiment must be enabled.";
  }
  FieldTrialParameter<int> min_packet_limit_ms("", min_packet_limit_.ms());
  ParseFieldTrial({&min_packet_limit_ms},
                  field_trials_->Lookup("WebRTC-Pacer-MinPacketLimitMs"));
  min_packet_limit_ = TimeDelta::ms(min_packet_limit_ms.Get());
  UpdateBudgetWithElapsedTime(min_packet_limit_);
}

PacedSender::~PacedSender() {}

void PacedSender::CreateProbeCluster(DataRate bitrate, int cluster_id) {
  rtc::CritScope cs(&critsect_);
  prober_.CreateProbeCluster(bitrate.bps(), CurrentTime().ms(), cluster_id);
}

void PacedSender::Pause() {
  {
    rtc::CritScope cs(&critsect_);
    if (!paused_)
      RTC_LOG(LS_INFO) << "PacedSender paused.";
    paused_ = true;
    packets_.SetPauseState(true, CurrentTime());
  }
  rtc::CritScope cs(&process_thread_lock_);
  // Tell the process thread to call our TimeUntilNextProcess() method to get
  // a new (longer) estimate for when to call Process().
  if (process_thread_)
    process_thread_->WakeUp(this);
}

void PacedSender::Resume() {
  {
    rtc::CritScope cs(&critsect_);
    if (paused_)
      RTC_LOG(LS_INFO) << "PacedSender resumed.";
    paused_ = false;
    packets_.SetPauseState(false, CurrentTime());
  }
  rtc::CritScope cs(&process_thread_lock_);
  // Tell the process thread to call our TimeUntilNextProcess() method to
  // refresh the estimate for when to call Process().
  if (process_thread_)
    process_thread_->WakeUp(this);
}

void PacedSender::SetCongestionWindow(DataSize congestion_window_size) {
  rtc::CritScope cs(&critsect_);
  congestion_window_size_ = congestion_window_size;
}

void PacedSender::UpdateOutstandingData(DataSize outstanding_data) {
  rtc::CritScope cs(&critsect_);
  outstanding_data_ = outstanding_data;
}

bool PacedSender::Congested() const {
  if (congestion_window_size_.IsFinite()) {
    return outstanding_data_ >= congestion_window_size_;
  }
  return false;
}

Timestamp PacedSender::CurrentTime() const {
  Timestamp time = clock_->CurrentTime();
  if (time < last_timestamp_) {
    RTC_LOG(LS_WARNING)
        << "Non-monotonic clock behavior observed. Previous timestamp: "
        << last_timestamp_.ms() << ", new timestamp: " << time.ms();
    RTC_DCHECK_GE(time, last_timestamp_);
    time = last_timestamp_;
  }
  last_timestamp_ = time;
  return time;
}

void PacedSender::SetProbingEnabled(bool enabled) {
  rtc::CritScope cs(&critsect_);
  RTC_CHECK_EQ(0, packet_counter_);
  prober_.SetEnabled(enabled);
}

void PacedSender::SetPacingRates(DataRate pacing_rate, DataRate padding_rate) {
  rtc::CritScope cs(&critsect_);
  RTC_DCHECK_GT(pacing_rate, DataRate::Zero());
  pacing_bitrate_ = pacing_rate;
  padding_budget_.set_target_rate_kbps(padding_rate.kbps());

  RTC_LOG(LS_VERBOSE) << "bwe:pacer_updated pacing_kbps="
                      << pacing_bitrate_.kbps()
                      << " padding_budget_kbps=" << padding_rate.kbps();
}

void PacedSender::InsertPacket(RtpPacketSender::Priority priority,
                               uint32_t ssrc,
                               uint16_t sequence_number,
                               int64_t capture_time_ms,
                               size_t bytes,
                               bool retransmission) {
  rtc::CritScope cs(&critsect_);
  RTC_DCHECK(pacing_bitrate_ > DataRate::Zero())
      << "SetPacingRate must be called before InsertPacket.";

  Timestamp now = CurrentTime();
  prober_.OnIncomingPacket(bytes);

  if (capture_time_ms < 0)
    capture_time_ms = now.ms();

  RtpPacketToSend::Type type;
  switch (priority) {
    case RtpPacketSender::kHighPriority:
      type = RtpPacketToSend::Type::kAudio;
      break;
    case RtpPacketSender::kNormalPriority:
      type = RtpPacketToSend::Type::kRetransmission;
      break;
    default:
      type = RtpPacketToSend::Type::kVideo;
  }
  packets_.Push(GetPriorityForType(type), type, ssrc, sequence_number,
                capture_time_ms, now, DataSize::bytes(bytes), retransmission,
                packet_counter_++);
}

void PacedSender::EnqueuePacket(std::unique_ptr<RtpPacketToSend> packet) {
  rtc::CritScope cs(&critsect_);
  RTC_DCHECK(pacing_bitrate_ > DataRate::Zero())
      << "SetPacingRate must be called before InsertPacket.";

  Timestamp now = CurrentTime();
  prober_.OnIncomingPacket(packet->payload_size());

  if (packet->capture_time_ms() < 0) {
    packet->set_capture_time_ms(now.ms());
  }

  RTC_CHECK(packet->packet_type());
  int priority = GetPriorityForType(*packet->packet_type());
  packets_.Push(priority, now, packet_counter_++, std::move(packet));
}

void PacedSender::SetAccountForAudioPackets(bool account_for_audio) {
  rtc::CritScope cs(&critsect_);
  account_for_audio_ = account_for_audio;
}

TimeDelta PacedSender::ExpectedQueueTime() const {
  rtc::CritScope cs(&critsect_);
  RTC_DCHECK_GT(pacing_bitrate_, DataRate::Zero());
  return TimeDelta::ms(
      (QueueSizeData().bytes() * 8 * rtc::kNumMillisecsPerSec) /
      pacing_bitrate_.bps());
}

size_t PacedSender::QueueSizePackets() const {
  rtc::CritScope cs(&critsect_);
  return packets_.SizeInPackets();
}

DataSize PacedSender::QueueSizeData() const {
  rtc::CritScope cs(&critsect_);
  return packets_.Size();
}

absl::optional<Timestamp> PacedSender::FirstSentPacketTime() const {
  rtc::CritScope cs(&critsect_);
  return first_sent_packet_time_;
}

TimeDelta PacedSender::OldestPacketWaitTime() const {
  rtc::CritScope cs(&critsect_);
  Timestamp oldest_packet = packets_.OldestEnqueueTime();
  if (oldest_packet.IsInfinite()) {
    return TimeDelta::Zero();
  }

  return CurrentTime() - oldest_packet;
}

int64_t PacedSender::TimeUntilNextProcess() {
  rtc::CritScope cs(&critsect_);
  TimeDelta elapsed_time = CurrentTime() - time_last_process_;
  // When paused we wake up every 500 ms to send a padding packet to ensure
  // we won't get stuck in the paused state due to no feedback being received.
  if (paused_) {
    return std::max(kPausedProcessInterval - elapsed_time, TimeDelta::Zero())
        .ms();
  }

  if (prober_.IsProbing()) {
    int64_t ret = prober_.TimeUntilNextProbe(CurrentTime().ms());
    if (ret > 0 || (ret == 0 && !probing_send_failure_))
      return ret;
  }
  return std::max(min_packet_limit_ - elapsed_time, TimeDelta::Zero()).ms();
}

TimeDelta PacedSender::UpdateTimeAndGetElapsed(Timestamp now) {
  TimeDelta elapsed_time = now - time_last_process_;
  time_last_process_ = now;
  if (elapsed_time > kMaxElapsedTime) {
    RTC_LOG(LS_WARNING) << "Elapsed time (" << elapsed_time.ms()
                        << " ms) longer than expected, limiting to "
                        << kMaxElapsedTime.ms();
    elapsed_time = kMaxElapsedTime;
  }
  return elapsed_time;
}

bool PacedSender::ShouldSendKeepalive(Timestamp now) const {
  if (send_padding_if_silent_ || paused_ || Congested()) {
    // We send a padding packet every 500 ms to ensure we won't get stuck in
    // congested state due to no feedback being received.
    TimeDelta elapsed_since_last_send = now - last_send_time_;
    if (elapsed_since_last_send >= kCongestedPacketInterval) {
      // We can not send padding unless a normal packet has first been sent. If
      // we do, timestamps get messed up.
      if (packet_counter_ > 0) {
        return true;
      }
    }
  }
  return false;
}

void PacedSender::Process() {
  rtc::CritScope cs(&critsect_);
  Timestamp now = CurrentTime();
  TimeDelta elapsed_time = UpdateTimeAndGetElapsed(now);
  if (ShouldSendKeepalive(now)) {
    if (legacy_packet_referencing_) {
      critsect_.Leave();
      size_t bytes_sent =
          packet_router_->TimeToSendPadding(1, PacedPacketInfo());
      critsect_.Enter();
      OnPaddingSent(DataSize::bytes(bytes_sent));
    } else {
      DataSize keepalive_data_sent = DataSize::Zero();
      critsect_.Leave();
      std::vector<std::unique_ptr<RtpPacketToSend>> keepalive_packets =
          packet_router_->GeneratePadding(1);
      for (auto& packet : keepalive_packets) {
        keepalive_data_sent +=
            DataSize::bytes(packet->payload_size() + packet->padding_size());
        packet_router_->SendPacket(std::move(packet), PacedPacketInfo());
      }
      critsect_.Enter();
      OnPaddingSent(keepalive_data_sent);
    }
  }

  if (paused_)
    return;

  if (elapsed_time > TimeDelta::Zero()) {
    DataRate target_rate = pacing_bitrate_;
    DataSize queue_size_data = packets_.Size();
    if (queue_size_data > DataSize::Zero()) {
      // Assuming equal size packets and input/output rate, the average packet
      // has avg_time_left_ms left to get queue_size_bytes out of the queue, if
      // time constraint shall be met. Determine bitrate needed for that.
      packets_.UpdateQueueTime(CurrentTime());
      if (drain_large_queues_) {
        TimeDelta avg_time_left = std::max(
            TimeDelta::ms(1), queue_time_limit - packets_.AverageQueueTime());
        DataRate min_rate_needed = queue_size_data / avg_time_left;
        if (min_rate_needed > target_rate) {
          target_rate = min_rate_needed;
          RTC_LOG(LS_VERBOSE) << "bwe:large_pacing_queue pacing_rate_kbps="
                              << target_rate.kbps();
        }
      }
    }

    media_budget_.set_target_rate_kbps(target_rate.kbps());
    UpdateBudgetWithElapsedTime(elapsed_time);
  }

  bool is_probing = prober_.IsProbing();
  PacedPacketInfo pacing_info;
  absl::optional<DataSize> recommended_probe_size;
  if (is_probing) {
    pacing_info = prober_.CurrentCluster();
    recommended_probe_size = DataSize::bytes(prober_.RecommendedMinProbeSize());
  }

  DataSize data_sent = DataSize::Zero();
  // The paused state is checked in the loop since it leaves the critical
  // section allowing the paused state to be changed from other code.
  while (!paused_) {
    auto* packet = GetPendingPacket(pacing_info);
    if (packet == nullptr) {
      // No packet available to send, check if we should send padding.
      if (!legacy_packet_referencing_) {
        DataSize padding_to_add =
            PaddingToAdd(recommended_probe_size, data_sent);
        if (padding_to_add > DataSize::Zero()) {
          critsect_.Leave();
          std::vector<std::unique_ptr<RtpPacketToSend>> padding_packets =
              packet_router_->GeneratePadding(padding_to_add.bytes());
          critsect_.Enter();
          if (padding_packets.empty()) {
            // No padding packets were generated, quite send loop.
            break;
          }
          for (auto& packet : padding_packets) {
            EnqueuePacket(std::move(packet));
          }
          // Continue loop to send the padding that was just added.
          continue;
        }
      }

      // Can't fetch new packet and no padding to send, exit send loop.
      break;
    }

    std::unique_ptr<RtpPacketToSend> rtp_packet = packet->ReleasePacket();
    const bool owned_rtp_packet = rtp_packet != nullptr;
    RtpPacketSendResult success;

    if (rtp_packet != nullptr) {
      critsect_.Leave();
      packet_router_->SendPacket(std::move(rtp_packet), pacing_info);
      critsect_.Enter();
      success = RtpPacketSendResult::kSuccess;
    } else {
      critsect_.Leave();
      success = packet_router_->TimeToSendPacket(
          packet->ssrc(), packet->sequence_number(), packet->capture_time_ms(),
          packet->is_retransmission(), pacing_info);
      critsect_.Enter();
    }

    if (success == RtpPacketSendResult::kSuccess ||
        success == RtpPacketSendResult::kPacketNotFound) {
      // Packet sent or invalid packet, remove it from queue.
      // TODO(webrtc:8052): Don't consume media budget on kInvalid.
      data_sent += packet->size();
      // Send succeeded, remove it from the queue.
      OnPacketSent(packet);
      if (recommended_probe_size && data_sent > *recommended_probe_size)
        break;
    } else if (owned_rtp_packet) {
      // Send failed, but we can't put it back in the queue, remove it without
      // consuming budget.
      packets_.FinalizePop();
      break;
    } else {
      // Send failed, put it back into the queue.
      packets_.CancelPop();
      break;
    }
  }

  if (legacy_packet_referencing_ && packets_.Empty() && !Congested()) {
    // We can not send padding unless a normal packet has first been sent. If we
    // do, timestamps get messed up.
    if (packet_counter_ > 0) {
      DataSize padding_needed =
          (recommended_probe_size && *recommended_probe_size > data_sent)
              ? (*recommended_probe_size - data_sent)
              : DataSize::bytes(padding_budget_.bytes_remaining());
      if (padding_needed > DataSize::Zero()) {
        DataSize padding_sent = DataSize::Zero();
        critsect_.Leave();
        padding_sent = DataSize::bytes(packet_router_->TimeToSendPadding(
            padding_needed.bytes(), pacing_info));
        critsect_.Enter();
        data_sent += padding_sent;
        OnPaddingSent(padding_sent);
      }
    }
  }

  if (is_probing) {
    probing_send_failure_ = data_sent == DataSize::Zero();
    if (!probing_send_failure_) {
      prober_.ProbeSent(CurrentTime().ms(), data_sent.bytes());
    }
  }
}

void PacedSender::ProcessThreadAttached(ProcessThread* process_thread) {
  RTC_LOG(LS_INFO) << "ProcessThreadAttached 0x" << process_thread;
  rtc::CritScope cs(&process_thread_lock_);
  process_thread_ = process_thread;
}

DataSize PacedSender::PaddingToAdd(
    absl::optional<DataSize> recommended_probe_size,
    DataSize data_sent) {
  if (!packets_.Empty()) {
    // Actual payload available, no need to add padding.
    return DataSize::Zero();
  }

  if (Congested()) {
    // Don't add padding if congested, even if requested for probing.
    return DataSize::Zero();
  }

  if (packet_counter_ == 0) {
    // We can not send padding unless a normal packet has first been sent. If we
    // do, timestamps get messed up.
    return DataSize::Zero();
  }

  if (recommended_probe_size) {
    if (*recommended_probe_size > data_sent) {
      return *recommended_probe_size - data_sent;
    }
    return DataSize::Zero();
  }

  return DataSize::bytes(padding_budget_.bytes_remaining());
}

RoundRobinPacketQueue::QueuedPacket* PacedSender::GetPendingPacket(
    const PacedPacketInfo& pacing_info) {
  if (packets_.Empty()) {
    return nullptr;
  }

  // Since we need to release the lock in order to send, we first pop the
  // element from the priority queue but keep it in storage, so that we can
  // reinsert it if send fails.
  RoundRobinPacketQueue::QueuedPacket* packet = packets_.BeginPop();
  bool audio_packet = packet->type() == RtpPacketToSend::Type::kAudio;
  bool apply_pacing = !audio_packet || pace_audio_;
  if (apply_pacing && (Congested() || (media_budget_.bytes_remaining() == 0 &&
                                       pacing_info.probe_cluster_id ==
                                           PacedPacketInfo::kNotAProbe))) {
    packets_.CancelPop();
    return nullptr;
  }
  return packet;
}

void PacedSender::OnPacketSent(RoundRobinPacketQueue::QueuedPacket* packet) {
  Timestamp now = CurrentTime();
  if (!first_sent_packet_time_) {
    first_sent_packet_time_ = now;
  }
  bool audio_packet = packet->type() == RtpPacketToSend::Type::kAudio;
  if (!audio_packet || account_for_audio_) {
    // Update media bytes sent.
    UpdateBudgetWithSentData(packet->size());
    last_send_time_ = now;
  }
  // Send succeeded, remove it from the queue.
  packets_.FinalizePop();
}

void PacedSender::OnPaddingSent(DataSize data_sent) {
  if (data_sent > DataSize::Zero()) {
    UpdateBudgetWithSentData(data_sent);
  }
  last_send_time_ = CurrentTime();
}

void PacedSender::UpdateBudgetWithElapsedTime(TimeDelta delta) {
  delta = std::min(kMaxProcessingInterval, delta);
  media_budget_.IncreaseBudget(delta.ms());
  padding_budget_.IncreaseBudget(delta.ms());
}

void PacedSender::UpdateBudgetWithSentData(DataSize size) {
  outstanding_data_ += size;
  media_budget_.UseBudget(size.bytes());
  padding_budget_.UseBudget(size.bytes());
}

void PacedSender::SetQueueTimeLimit(TimeDelta limit) {
  rtc::CritScope cs(&critsect_);
  queue_time_limit = limit;
}

}  // namespace webrtc
