/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/include/send_side_congestion_controller.h"

#include <inttypes.h>
#include <algorithm>
#include <cstdio>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "api/units/data_rate.h"
#include "api/units/timestamp.h"
#include "modules/bitrate_controller/include/bitrate_controller.h"
#include "modules/congestion_controller/goog_cc/acknowledged_bitrate_estimator.h"
#include "modules/congestion_controller/goog_cc/congestion_window_pushback_controller.h"
#include "modules/congestion_controller/goog_cc/probe_controller.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/rate_control_settings.h"
#include "rtc_base/logging.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/rate_limiter.h"
#include "rtc_base/time_utils.h"

namespace webrtc {
namespace {

static const int64_t kRetransmitWindowSizeMs = 500;

// Makes sure that the bitrate and the min, max values are in valid range.
static void ClampBitrates(int* bitrate_bps,
                          int* min_bitrate_bps,
                          int* max_bitrate_bps) {
  // TODO(holmer): We should make sure the default bitrates are set to 10 kbps,
  // and that we don't try to set the min bitrate to 0 from any applications.
  // The congestion controller should allow a min bitrate of 0.
  if (*min_bitrate_bps < congestion_controller::GetMinBitrateBps())
    *min_bitrate_bps = congestion_controller::GetMinBitrateBps();
  if (*max_bitrate_bps > 0)
    *max_bitrate_bps = std::max(*min_bitrate_bps, *max_bitrate_bps);
  if (*bitrate_bps > 0)
    *bitrate_bps = std::max(*min_bitrate_bps, *bitrate_bps);
}

std::vector<webrtc::PacketFeedback> ReceivedPacketFeedbackVector(
    const std::vector<webrtc::PacketFeedback>& input) {
  std::vector<PacketFeedback> received_packet_feedback_vector;
  auto is_received = [](const webrtc::PacketFeedback& packet_feedback) {
    return packet_feedback.arrival_time_ms !=
           webrtc::PacketFeedback::kNotReceived;
  };
  std::copy_if(input.begin(), input.end(),
               std::back_inserter(received_packet_feedback_vector),
               is_received);
  return received_packet_feedback_vector;
}

void SortPacketFeedbackVector(
    std::vector<webrtc::PacketFeedback>* const input) {
  RTC_DCHECK(input);
  std::sort(input->begin(), input->end(), PacketFeedbackComparator());
}

}  // namespace

DEPRECATED_SendSideCongestionController::
    DEPRECATED_SendSideCongestionController(
        Clock* clock,
        Observer* observer,
        RtcEventLog* event_log,
        PacedSender* pacer,
        const WebRtcKeyValueConfig* key_value_config)
    : key_value_config_(key_value_config ? key_value_config
                                         : &field_trial_config_),
      clock_(clock),
      observer_(observer),
      event_log_(event_log),
      pacer_(pacer),
      bitrate_controller_(
          BitrateController::CreateBitrateController(clock_, event_log)),
      acknowledged_bitrate_estimator_(
          absl::make_unique<AcknowledgedBitrateEstimator>(key_value_config_)),
      probe_controller_(new ProbeController(key_value_config_, event_log)),
      retransmission_rate_limiter_(
          new RateLimiter(clock, kRetransmitWindowSizeMs)),
      transport_feedback_adapter_(clock_),
      last_reported_bitrate_bps_(0),
      last_reported_fraction_loss_(0),
      last_reported_rtt_(0),
      network_state_(kNetworkUp),
      pause_pacer_(false),
      pacer_paused_(false),
      min_bitrate_bps_(congestion_controller::GetMinBitrateBps()),
      probe_bitrate_estimator_(new ProbeBitrateEstimator(event_log_)),
      delay_based_bwe_(
          new DelayBasedBwe(key_value_config_, event_log_, nullptr)),
      was_in_alr_(false),
      send_side_bwe_with_overhead_(
          key_value_config_->Lookup("WebRTC-SendSideBwe-WithOverhead")
              .find("Enabled") == 0),
      transport_overhead_bytes_per_packet_(0) {
  RateControlSettings experiment_params =
      RateControlSettings::ParseFromKeyValueConfig(key_value_config);
  if (experiment_params.UseCongestionWindow()) {
    cwnd_experiment_parameter_ =
        experiment_params.GetCongestionWindowAdditionalTimeMs();
  }
  if (experiment_params.UseCongestionWindowPushback()) {
    congestion_window_pushback_controller_ =
        absl::make_unique<CongestionWindowPushbackController>(
            key_value_config_);
  }
  delay_based_bwe_->SetMinBitrate(DataRate::bps(min_bitrate_bps_));
}

DEPRECATED_SendSideCongestionController::
    ~DEPRECATED_SendSideCongestionController() {}

void DEPRECATED_SendSideCongestionController::EnableCongestionWindowPushback(
    int64_t accepted_queue_ms,
    uint32_t min_pushback_target_bitrate_bps) {
  RTC_DCHECK(!congestion_window_pushback_controller_)
      << "The congestion pushback is already enabled.";
  RTC_CHECK_GE(accepted_queue_ms, 0)
      << "Accepted must be greater than or equal to 0.";
  RTC_CHECK_GE(min_pushback_target_bitrate_bps, 0)
      << "Min pushback target bitrate must be greater than or equal to 0.";

  cwnd_experiment_parameter_ = accepted_queue_ms;
  congestion_window_pushback_controller_ =
      absl::make_unique<CongestionWindowPushbackController>(
          key_value_config_, min_pushback_target_bitrate_bps);
}

void DEPRECATED_SendSideCongestionController::SetAlrLimitedBackoffExperiment(
    bool enable) {
  rtc::CritScope cs(&bwe_lock_);
  delay_based_bwe_->SetAlrLimitedBackoffExperiment(enable);
}

void DEPRECATED_SendSideCongestionController::SetMaxProbingBitrate(
    int64_t max_probing_bitrate_bps) {
  rtc::CritScope cs(&probe_lock_);
  probe_controller_->SetMaxBitrate(max_probing_bitrate_bps);
}

void DEPRECATED_SendSideCongestionController::RegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  transport_feedback_adapter_.RegisterPacketFeedbackObserver(observer);
}

void DEPRECATED_SendSideCongestionController::DeRegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  transport_feedback_adapter_.DeRegisterPacketFeedbackObserver(observer);
}

void DEPRECATED_SendSideCongestionController::RegisterNetworkObserver(
    Observer* observer) {
  rtc::CritScope cs(&observer_lock_);
  RTC_DCHECK(observer_ == nullptr);
  observer_ = observer;
}

void DEPRECATED_SendSideCongestionController::DeRegisterNetworkObserver(
    Observer* observer) {
  rtc::CritScope cs(&observer_lock_);
  RTC_DCHECK_EQ(observer_, observer);
  observer_ = nullptr;
}

void DEPRECATED_SendSideCongestionController::SetBweBitrates(
    int min_bitrate_bps,
    int start_bitrate_bps,
    int max_bitrate_bps) {
  ClampBitrates(&start_bitrate_bps, &min_bitrate_bps, &max_bitrate_bps);
  bitrate_controller_->SetBitrates(start_bitrate_bps, min_bitrate_bps,
                                   max_bitrate_bps);

  {
    rtc::CritScope cs(&probe_lock_);
    SendProbes(probe_controller_->SetBitrates(
        min_bitrate_bps, start_bitrate_bps, max_bitrate_bps,
        clock_->TimeInMilliseconds()));
  }

  {
    rtc::CritScope cs(&bwe_lock_);
    if (start_bitrate_bps > 0)
      delay_based_bwe_->SetStartBitrate(DataRate::bps(start_bitrate_bps));
    min_bitrate_bps_ = min_bitrate_bps;
    delay_based_bwe_->SetMinBitrate(DataRate::bps(min_bitrate_bps_));
  }
  MaybeTriggerOnNetworkChanged();
}

void DEPRECATED_SendSideCongestionController::SetAllocatedSendBitrateLimits(
    int64_t min_send_bitrate_bps,
    int64_t max_padding_bitrate_bps,
    int64_t max_total_bitrate_bps) {
  pacer_->SetSendBitrateLimits(min_send_bitrate_bps, max_padding_bitrate_bps);

  rtc::CritScope cs(&probe_lock_);
  SendProbes(probe_controller_->OnMaxTotalAllocatedBitrate(
      max_total_bitrate_bps, clock_->TimeInMilliseconds()));
}

// TODO(holmer): Split this up and use SetBweBitrates in combination with
// OnNetworkRouteChanged.
void DEPRECATED_SendSideCongestionController::OnNetworkRouteChanged(
    const rtc::NetworkRoute& network_route,
    int bitrate_bps,
    int min_bitrate_bps,
    int max_bitrate_bps) {
  ClampBitrates(&bitrate_bps, &min_bitrate_bps, &max_bitrate_bps);
  // TODO(honghaiz): Recreate this object once the bitrate controller is
  // no longer exposed outside SendSideCongestionController.
  bitrate_controller_->ResetBitrates(bitrate_bps, min_bitrate_bps,
                                     max_bitrate_bps);

  transport_feedback_adapter_.SetNetworkIds(network_route.local_network_id,
                                            network_route.remote_network_id);
  {
    rtc::CritScope cs(&bwe_lock_);
    transport_overhead_bytes_per_packet_ = network_route.packet_overhead;
    min_bitrate_bps_ = min_bitrate_bps;
    probe_bitrate_estimator_.reset(new ProbeBitrateEstimator(event_log_));
    delay_based_bwe_.reset(
        new DelayBasedBwe(key_value_config_, event_log_, nullptr));
    acknowledged_bitrate_estimator_.reset(
        new AcknowledgedBitrateEstimator(key_value_config_));
    if (bitrate_bps > 0) {
      delay_based_bwe_->SetStartBitrate(DataRate::bps(bitrate_bps));
    }
    delay_based_bwe_->SetMinBitrate(DataRate::bps(min_bitrate_bps));
  }
  {
    rtc::CritScope cs(&probe_lock_);
    probe_controller_->Reset(clock_->TimeInMilliseconds());
    SendProbes(probe_controller_->SetBitrates(min_bitrate_bps, bitrate_bps,
                                              max_bitrate_bps,
                                              clock_->TimeInMilliseconds()));
  }

  MaybeTriggerOnNetworkChanged();
}

bool DEPRECATED_SendSideCongestionController::AvailableBandwidth(
    uint32_t* bandwidth) const {
  return bitrate_controller_->AvailableBandwidth(bandwidth);
}

RtcpBandwidthObserver*
DEPRECATED_SendSideCongestionController::GetBandwidthObserver() {
  return bitrate_controller_.get();
}

void DEPRECATED_SendSideCongestionController::SetPerPacketFeedbackAvailable(
    bool available) {}

void DEPRECATED_SendSideCongestionController::EnablePeriodicAlrProbing(
    bool enable) {
  rtc::CritScope cs(&probe_lock_);
  probe_controller_->EnablePeriodicAlrProbing(enable);
}

int64_t DEPRECATED_SendSideCongestionController::GetPacerQueuingDelayMs()
    const {
  return IsNetworkDown() ? 0 : pacer_->QueueInMs();
}

int64_t DEPRECATED_SendSideCongestionController::GetFirstPacketTimeMs() const {
  return pacer_->FirstSentPacketTimeMs();
}

TransportFeedbackObserver*
DEPRECATED_SendSideCongestionController::GetTransportFeedbackObserver() {
  return this;
}

void DEPRECATED_SendSideCongestionController::SignalNetworkState(
    NetworkState state) {
  RTC_LOG(LS_INFO) << "SignalNetworkState "
                   << (state == kNetworkUp ? "Up" : "Down");
  {
    rtc::CritScope cs(&network_state_lock_);
    pause_pacer_ = state == kNetworkDown;
    network_state_ = state;
  }

  {
    rtc::CritScope cs(&probe_lock_);
    NetworkAvailability msg;
    msg.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
    msg.network_available = state == kNetworkUp;
    SendProbes(probe_controller_->OnNetworkAvailability(msg));
  }
  MaybeTriggerOnNetworkChanged();
}

void DEPRECATED_SendSideCongestionController::OnSentPacket(
    const rtc::SentPacket& sent_packet) {
  // We're not interested in packets without an id, which may be stun packets,
  // etc, sent on the same transport.
  if (sent_packet.packet_id == -1)
    return;
  transport_feedback_adapter_.OnSentPacket(sent_packet.packet_id,
                                           sent_packet.send_time_ms);
  if (cwnd_experiment_parameter_)
    LimitOutstandingBytes(transport_feedback_adapter_.GetOutstandingBytes());
}

void DEPRECATED_SendSideCongestionController::OnRttUpdate(int64_t avg_rtt_ms,
                                                          int64_t max_rtt_ms) {
  rtc::CritScope cs(&bwe_lock_);
  delay_based_bwe_->OnRttUpdate(TimeDelta::ms(avg_rtt_ms));
}

int64_t DEPRECATED_SendSideCongestionController::TimeUntilNextProcess() {
  return bitrate_controller_->TimeUntilNextProcess();
}

void DEPRECATED_SendSideCongestionController::SendProbes(
    std::vector<ProbeClusterConfig> probe_configs) {
  for (auto probe_config : probe_configs) {
    pacer_->CreateProbeCluster(probe_config.target_data_rate.bps(),
                               probe_config.id);
  }
}

void DEPRECATED_SendSideCongestionController::Process() {
  bool pause_pacer;
  // TODO(holmer): Once this class is running on a task queue we should
  // replace this with a task instead.
  {
    rtc::CritScope lock(&network_state_lock_);
    pause_pacer = pause_pacer_;
  }
  if (pause_pacer && !pacer_paused_) {
    pacer_->Pause();
    pacer_paused_ = true;
  } else if (!pause_pacer && pacer_paused_) {
    pacer_->Resume();
    pacer_paused_ = false;
  }
  bitrate_controller_->Process();

  {
    rtc::CritScope cs(&probe_lock_);
    probe_controller_->SetAlrStartTimeMs(
        pacer_->GetApplicationLimitedRegionStartTime());
    SendProbes(probe_controller_->Process(clock_->TimeInMilliseconds()));
  }
  MaybeTriggerOnNetworkChanged();
}

void DEPRECATED_SendSideCongestionController::OnAddPacket(
    const RtpPacketSendInfo& packet_info) {
  size_t overhead_bytes = 0;
  if (send_side_bwe_with_overhead_) {
    rtc::CritScope cs(&bwe_lock_);
    overhead_bytes = transport_overhead_bytes_per_packet_;
  }
  transport_feedback_adapter_.AddPacket(
      packet_info.ssrc, packet_info.transport_sequence_number,
      packet_info.length + overhead_bytes, packet_info.pacing_info);
}

void DEPRECATED_SendSideCongestionController::OnTransportFeedback(
    const rtcp::TransportFeedback& feedback) {
  RTC_DCHECK_RUNS_SERIALIZED(&worker_race_);
  transport_feedback_adapter_.OnTransportFeedback(feedback);
  std::vector<PacketFeedback> feedback_vector = ReceivedPacketFeedbackVector(
      transport_feedback_adapter_.GetTransportFeedbackVector());
  SortPacketFeedbackVector(&feedback_vector);

  bool currently_in_alr =
      pacer_->GetApplicationLimitedRegionStartTime().has_value();
  if (was_in_alr_ && !currently_in_alr) {
    int64_t now_ms = rtc::TimeMillis();
    acknowledged_bitrate_estimator_->SetAlrEndedTimeMs(now_ms);
    rtc::CritScope cs(&probe_lock_);
    probe_controller_->SetAlrEndedTimeMs(now_ms);
  }
  was_in_alr_ = currently_in_alr;

  acknowledged_bitrate_estimator_->IncomingPacketFeedbackVector(
      feedback_vector);
  DelayBasedBwe::Result result;
  {
    rtc::CritScope cs(&bwe_lock_);
    for (const auto& packet : feedback_vector) {
      if (packet.send_time_ms != PacketFeedback::kNoSendTime &&
          packet.pacing_info.probe_cluster_id != PacedPacketInfo::kNotAProbe) {
        probe_bitrate_estimator_->HandleProbeAndEstimateBitrate(packet);
      }
    }
    result = delay_based_bwe_->IncomingPacketFeedbackVector(
        feedback_vector, acknowledged_bitrate_estimator_->bitrate(),
        probe_bitrate_estimator_->FetchAndResetLastEstimatedBitrate(),
        absl::nullopt, currently_in_alr,
        Timestamp::ms(clock_->TimeInMilliseconds()));
  }
  if (result.updated) {
    bitrate_controller_->OnDelayBasedBweResult(result);
    // Update the estimate in the ProbeController, in case we want to probe.
    MaybeTriggerOnNetworkChanged();
  }
  if (result.recovered_from_overuse) {
    rtc::CritScope cs(&probe_lock_);
    probe_controller_->SetAlrStartTimeMs(
        pacer_->GetApplicationLimitedRegionStartTime());
    SendProbes(probe_controller_->RequestProbe(clock_->TimeInMilliseconds()));
  } else if (result.backoff_in_alr) {
    rtc::CritScope cs(&probe_lock_);
    SendProbes(probe_controller_->RequestProbe(clock_->TimeInMilliseconds()));
  }
  if (cwnd_experiment_parameter_) {
    LimitOutstandingBytes(transport_feedback_adapter_.GetOutstandingBytes());
  }
}

void DEPRECATED_SendSideCongestionController::LimitOutstandingBytes(
    size_t num_outstanding_bytes) {
  RTC_DCHECK(cwnd_experiment_parameter_);
  rtc::CritScope lock(&network_state_lock_);
  absl::optional<int64_t> min_rtt_ms =
      transport_feedback_adapter_.GetMinFeedbackLoopRtt();
  // No valid RTT. Could be because send-side BWE isn't used, in which case
  // we don't try to limit the outstanding packets.
  if (!min_rtt_ms)
    return;
  const size_t kMinCwndBytes = 2 * 1500;
  size_t max_outstanding_bytes =
      std::max<size_t>((*min_rtt_ms + *cwnd_experiment_parameter_) *
                           last_reported_bitrate_bps_ / 1000 / 8,
                       kMinCwndBytes);
  if (congestion_window_pushback_controller_) {
    congestion_window_pushback_controller_->UpdateOutstandingData(
        num_outstanding_bytes);
    congestion_window_pushback_controller_->UpdateMaxOutstandingData(
        max_outstanding_bytes);
  } else {
    pause_pacer_ = num_outstanding_bytes > max_outstanding_bytes;
  }
}

std::vector<PacketFeedback>
DEPRECATED_SendSideCongestionController::GetTransportFeedbackVector() const {
  RTC_DCHECK_RUNS_SERIALIZED(&worker_race_);
  return transport_feedback_adapter_.GetTransportFeedbackVector();
}

void DEPRECATED_SendSideCongestionController::SetPacingFactor(
    float pacing_factor) {
  pacer_->SetPacingFactor(pacing_factor);
}

void DEPRECATED_SendSideCongestionController::
    SetAllocatedBitrateWithoutFeedback(uint32_t bitrate_bps) {
}

void DEPRECATED_SendSideCongestionController::MaybeTriggerOnNetworkChanged() {
  uint32_t bitrate_bps;
  uint8_t fraction_loss;
  int64_t rtt;
  bool estimate_changed = bitrate_controller_->GetNetworkParameters(
      &bitrate_bps, &fraction_loss, &rtt);
  if (estimate_changed) {
    pacer_->SetEstimatedBitrate(bitrate_bps);
    {
      rtc::CritScope cs(&probe_lock_);
      SendProbes(probe_controller_->SetEstimatedBitrate(
          bitrate_bps, clock_->TimeInMilliseconds()));
    }
    retransmission_rate_limiter_->SetMaxRate(bitrate_bps);
  }

  if (IsNetworkDown()) {
    bitrate_bps = 0;
  } else if (congestion_window_pushback_controller_) {
    rtc::CritScope lock(&network_state_lock_);
    bitrate_bps = congestion_window_pushback_controller_->UpdateTargetBitrate(
        bitrate_bps);
  } else {
    bitrate_bps = IsSendQueueFull() ? 0 : bitrate_bps;
  }

  if (HasNetworkParametersToReportChanged(bitrate_bps, fraction_loss, rtt)) {
    int64_t probing_interval_ms;
    {
      rtc::CritScope cs(&bwe_lock_);
      probing_interval_ms = delay_based_bwe_->GetExpectedBwePeriod().ms();
    }
    {
      rtc::CritScope cs(&observer_lock_);
      if (observer_) {
        observer_->OnNetworkChanged(bitrate_bps, fraction_loss, rtt,
                                    probing_interval_ms);
      }
    }
  }
}

bool DEPRECATED_SendSideCongestionController::
    HasNetworkParametersToReportChanged(uint32_t bitrate_bps,
                                        uint8_t fraction_loss,
                                        int64_t rtt) {
  rtc::CritScope cs(&network_state_lock_);
  bool changed =
      last_reported_bitrate_bps_ != bitrate_bps ||
      (bitrate_bps > 0 && (last_reported_fraction_loss_ != fraction_loss ||
                           last_reported_rtt_ != rtt));
  if (changed && (last_reported_bitrate_bps_ == 0 || bitrate_bps == 0)) {
    RTC_LOG(LS_INFO) << "Bitrate estimate state changed, BWE: " << bitrate_bps
                     << " bps.";
  }
  last_reported_bitrate_bps_ = bitrate_bps;
  last_reported_fraction_loss_ = fraction_loss;
  last_reported_rtt_ = rtt;
  return changed;
}

bool DEPRECATED_SendSideCongestionController::IsSendQueueFull() const {
  return pacer_->ExpectedQueueTimeMs() > PacedSender::kMaxQueueLengthMs;
}

bool DEPRECATED_SendSideCongestionController::IsNetworkDown() const {
  rtc::CritScope cs(&network_state_lock_);
  return network_state_ == kNetworkDown;
}

}  // namespace webrtc
