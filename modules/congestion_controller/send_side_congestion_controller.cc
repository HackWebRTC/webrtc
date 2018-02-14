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

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>
#include "modules/congestion_controller/include/goog_cc_factory.h"
#include "modules/congestion_controller/network_control/include/network_types.h"
#include "modules/congestion_controller/network_control/include/network_units.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "rtc_base/checks.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/rate_limiter.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/socket.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/runtime_enabled_features.h"

using rtc::MakeUnique;

namespace webrtc {
namespace {

static const int64_t kRetransmitWindowSizeMs = 500;

const char kPacerPushbackExperiment[] = "WebRTC-PacerPushbackExperiment";

bool IsPacerPushbackExperimentEnabled() {
  return webrtc::field_trial::IsEnabled(kPacerPushbackExperiment) ||
         (!webrtc::field_trial::IsDisabled(kPacerPushbackExperiment) &&
          webrtc::runtime_enabled_features::IsFeatureEnabled(
              webrtc::runtime_enabled_features::kDualStreamModeFeatureName));
}

NetworkControllerFactoryInterface::uptr ControllerFactory(
    RtcEventLog* event_log) {
  return rtc::MakeUnique<GoogCcNetworkControllerFactory>(event_log);
}

void SortPacketFeedbackVector(std::vector<webrtc::PacketFeedback>* input) {
  std::sort(input->begin(), input->end(), PacketFeedbackComparator());
}

PacketResult NetworkPacketFeedbackFromRtpPacketFeedback(
    const webrtc::PacketFeedback& pf) {
  PacketResult feedback;
  if (pf.arrival_time_ms == webrtc::PacketFeedback::kNotReceived)
    feedback.receive_time = Timestamp::Infinity();
  else
    feedback.receive_time = Timestamp::ms(pf.arrival_time_ms);
  if (pf.send_time_ms != webrtc::PacketFeedback::kNoSendTime) {
    feedback.sent_packet = SentPacket();
    feedback.sent_packet->send_time = Timestamp::ms(pf.send_time_ms);
    feedback.sent_packet->size = DataSize::bytes(pf.payload_size);
    feedback.sent_packet->pacing_info = pf.pacing_info;
  }
  return feedback;
}

std::vector<PacketResult> PacketResultsFromRtpFeedbackVector(
    const std::vector<PacketFeedback>& feedback_vector) {
  RTC_DCHECK(std::is_sorted(feedback_vector.begin(), feedback_vector.end(),
                            PacketFeedbackComparator()));

  std::vector<PacketResult> packet_feedbacks;
  packet_feedbacks.reserve(feedback_vector.size());
  for (const PacketFeedback& rtp_feedback : feedback_vector) {
    auto feedback = NetworkPacketFeedbackFromRtpPacketFeedback(rtp_feedback);
    packet_feedbacks.push_back(feedback);
  }
  return packet_feedbacks;
}

TargetRateConstraints ConvertConstraints(int min_bitrate_bps,
                                         int max_bitrate_bps,
                                         int start_bitrate_bps,
                                         const Clock* clock) {
  TargetRateConstraints msg;
  msg.at_time = Timestamp::ms(clock->TimeInMilliseconds());
  msg.min_data_rate =
      min_bitrate_bps >= 0 ? DataRate::bps(min_bitrate_bps) : DataRate::Zero();
  msg.starting_rate = start_bitrate_bps > 0 ? DataRate::bps(start_bitrate_bps)
                                            : DataRate::kNotInitialized;
  msg.max_data_rate = max_bitrate_bps > 0 ? DataRate::bps(max_bitrate_bps)
                                          : DataRate::Infinity();
  return msg;
}
}  // namespace

namespace send_side_cc_internal {
class ControlHandler : public NetworkControllerObserver {
 public:
  ControlHandler(PacerController* pacer_controller, const Clock* clock);

  void OnCongestionWindow(CongestionWindow window) override;
  void OnPacerConfig(PacerConfig config) override;
  void OnProbeClusterConfig(ProbeClusterConfig config) override;
  void OnTargetTransferRate(TargetTransferRate target_rate) override;

  void OnNetworkAvailability(NetworkAvailability msg);
  void OnPacerQueueUpdate(PacerQueueUpdate msg);

  void RegisterNetworkObserver(
      SendSideCongestionController::Observer* observer);
  void DeRegisterNetworkObserver(
      SendSideCongestionController::Observer* observer);

  rtc::Optional<TargetTransferRate> last_transfer_rate();
  bool pacer_configured();
  RateLimiter* retransmission_rate_limiter();

 private:
  void OnNetworkInvalidation();
  bool GetNetworkParameters(int32_t* estimated_bitrate_bps,
                            uint8_t* fraction_loss,
                            int64_t* rtt_ms);
  bool IsSendQueueFull() const;
  bool HasNetworkParametersToReportChanged(int64_t bitrate_bps,
                                           uint8_t fraction_loss,
                                           int64_t rtt);
  PacerController* pacer_controller_;
  RateLimiter retransmission_rate_limiter_;

  rtc::CriticalSection state_lock_;
  rtc::Optional<TargetTransferRate> last_target_rate_
      RTC_GUARDED_BY(state_lock_);
  bool pacer_configured_ RTC_GUARDED_BY(state_lock_) = false;

  SendSideCongestionController::Observer* observer_ = nullptr;
  rtc::Optional<TargetTransferRate> current_target_rate_msg_;
  bool network_available_ = true;
  int64_t last_reported_target_bitrate_bps_ = 0;
  uint8_t last_reported_fraction_loss_ = 0;
  int64_t last_reported_rtt_ms_ = 0;
  const bool pacer_pushback_experiment_ = false;
  int64_t pacer_expected_queue_ms_ = 0;
  float encoding_rate_ratio_ = 1.0;

  rtc::SequencedTaskChecker sequenced_checker_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(ControlHandler);
};

ControlHandler::ControlHandler(PacerController* pacer_controller,
                               const Clock* clock)
    : pacer_controller_(pacer_controller),
      retransmission_rate_limiter_(clock, kRetransmitWindowSizeMs),
      pacer_pushback_experiment_(IsPacerPushbackExperimentEnabled()) {
  sequenced_checker_.Detach();
}

void ControlHandler::OnCongestionWindow(CongestionWindow window) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  pacer_controller_->OnCongestionWindow(window);
}

void ControlHandler::OnPacerConfig(PacerConfig config) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  pacer_controller_->OnPacerConfig(config);
  rtc::CritScope cs(&state_lock_);
  pacer_configured_ = true;
}

void ControlHandler::OnProbeClusterConfig(ProbeClusterConfig config) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  pacer_controller_->OnProbeClusterConfig(config);
}

void ControlHandler::OnTargetTransferRate(TargetTransferRate target_rate) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  retransmission_rate_limiter_.SetMaxRate(
      target_rate.network_estimate.bandwidth.bps());

  current_target_rate_msg_ = target_rate;
  OnNetworkInvalidation();
  rtc::CritScope cs(&state_lock_);
  last_target_rate_ = target_rate;
}

void ControlHandler::OnNetworkAvailability(NetworkAvailability msg) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  network_available_ = msg.network_available;
  OnNetworkInvalidation();
}

void ControlHandler::OnPacerQueueUpdate(PacerQueueUpdate msg) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  pacer_expected_queue_ms_ = msg.expected_queue_time.ms();
  OnNetworkInvalidation();
}

void ControlHandler::RegisterNetworkObserver(
    SendSideCongestionController::Observer* observer) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  RTC_DCHECK(observer_ == nullptr);
  observer_ = observer;
}

void ControlHandler::DeRegisterNetworkObserver(
    SendSideCongestionController::Observer* observer) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  RTC_DCHECK_EQ(observer_, observer);
  observer_ = nullptr;
}

void ControlHandler::OnNetworkInvalidation() {
  if (!current_target_rate_msg_.has_value())
    return;

  uint32_t target_bitrate_bps = current_target_rate_msg_->target_rate.bps();
  int64_t rtt_ms =
      current_target_rate_msg_->network_estimate.round_trip_time.ms();
  float loss_rate_ratio =
      current_target_rate_msg_->network_estimate.loss_rate_ratio;

  int loss_ratio_255 = loss_rate_ratio * 255;
  uint8_t fraction_loss =
      rtc::dchecked_cast<uint8_t>(rtc::SafeClamp(loss_ratio_255, 0, 255));

  int64_t probing_interval_ms =
      current_target_rate_msg_->network_estimate.bwe_period.ms();

  if (!network_available_) {
    target_bitrate_bps = 0;
  } else if (!pacer_pushback_experiment_) {
    target_bitrate_bps = IsSendQueueFull() ? 0 : target_bitrate_bps;
  } else {
    int64_t queue_length_ms = pacer_expected_queue_ms_;

    if (queue_length_ms == 0) {
      encoding_rate_ratio_ = 1.0;
    } else if (queue_length_ms > 50) {
      float encoding_ratio = 1.0 - queue_length_ms / 1000.0;
      encoding_rate_ratio_ = std::min(encoding_rate_ratio_, encoding_ratio);
      encoding_rate_ratio_ = std::max(encoding_rate_ratio_, 0.0f);
    }

    target_bitrate_bps *= encoding_rate_ratio_;
    target_bitrate_bps = target_bitrate_bps < 50000 ? 0 : target_bitrate_bps;
  }
  if (HasNetworkParametersToReportChanged(target_bitrate_bps, fraction_loss,
                                          rtt_ms)) {
    if (observer_) {
      observer_->OnNetworkChanged(target_bitrate_bps, fraction_loss, rtt_ms,
                                  probing_interval_ms);
    }
  }
}
bool ControlHandler::HasNetworkParametersToReportChanged(
    int64_t target_bitrate_bps,
    uint8_t fraction_loss,
    int64_t rtt_ms) {
  bool changed = last_reported_target_bitrate_bps_ != target_bitrate_bps ||
                 (target_bitrate_bps > 0 &&
                  (last_reported_fraction_loss_ != fraction_loss ||
                   last_reported_rtt_ms_ != rtt_ms));
  if (changed &&
      (last_reported_target_bitrate_bps_ == 0 || target_bitrate_bps == 0)) {
    RTC_LOG(LS_INFO) << "Bitrate estimate state changed, BWE: "
                     << target_bitrate_bps << " bps.";
  }
  last_reported_target_bitrate_bps_ = target_bitrate_bps;
  last_reported_fraction_loss_ = fraction_loss;
  last_reported_rtt_ms_ = rtt_ms;
  return changed;
}

bool ControlHandler::IsSendQueueFull() const {
  return pacer_expected_queue_ms_ > PacedSender::kMaxQueueLengthMs;
}

rtc::Optional<TargetTransferRate> ControlHandler::last_transfer_rate() {
  rtc::CritScope cs(&state_lock_);
  return last_target_rate_;
}

bool ControlHandler::pacer_configured() {
  rtc::CritScope cs(&state_lock_);
  return pacer_configured_;
}

RateLimiter* ControlHandler::retransmission_rate_limiter() {
  return &retransmission_rate_limiter_;
}
}  // namespace send_side_cc_internal

SendSideCongestionController::SendSideCongestionController(
    const Clock* clock,
    Observer* observer,
    RtcEventLog* event_log,
    PacedSender* pacer)
    : SendSideCongestionController(clock,
                                   event_log,
                                   pacer,
                                   ControllerFactory(event_log)) {
  if (observer != nullptr)
    RegisterNetworkObserver(observer);
}

SendSideCongestionController::SendSideCongestionController(
    const Clock* clock,
    RtcEventLog* event_log,
    PacedSender* pacer,
    NetworkControllerFactoryInterface::uptr controller_factory)
    : clock_(clock),
      pacer_(pacer),
      transport_feedback_adapter_(clock_),
      pacer_controller_(MakeUnique<PacerController>(pacer_)),
      control_handler(MakeUnique<send_side_cc_internal::ControlHandler>(
          pacer_controller_.get(),
          clock_)),
      controller_(controller_factory->Create(control_handler.get())),
      process_interval_(controller_factory->GetProcessInterval()),
      send_side_bwe_with_overhead_(
          webrtc::field_trial::IsEnabled("WebRTC-SendSideBwe-WithOverhead")),
      transport_overhead_bytes_per_packet_(0),
      network_available_(true),
      task_queue_(MakeUnique<rtc::TaskQueue>("SendSideCCQueue")) {}

SendSideCongestionController::~SendSideCongestionController() {
  // Must be destructed before any objects used by calls on the task queue.
  task_queue_.reset();
}

void SendSideCongestionController::RegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  transport_feedback_adapter_.RegisterPacketFeedbackObserver(observer);
}

void SendSideCongestionController::DeRegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  transport_feedback_adapter_.DeRegisterPacketFeedbackObserver(observer);
}

void SendSideCongestionController::RegisterNetworkObserver(Observer* observer) {
  WaitOnTask([this, observer]() {
    control_handler->RegisterNetworkObserver(observer);
  });
}

void SendSideCongestionController::DeRegisterNetworkObserver(
    Observer* observer) {
  WaitOnTask([this, observer]() {
    control_handler->DeRegisterNetworkObserver(observer);
  });
}

void SendSideCongestionController::SetBweBitrates(int min_bitrate_bps,
                                                  int start_bitrate_bps,
                                                  int max_bitrate_bps) {
  TargetRateConstraints msg = ConvertConstraints(
      min_bitrate_bps, max_bitrate_bps, start_bitrate_bps, clock_);
  WaitOnTask([this, msg]() { controller_->OnTargetRateConstraints(msg); });
}

// TODO(holmer): Split this up and use SetBweBitrates in combination with
// OnNetworkRouteChanged.
void SendSideCongestionController::OnNetworkRouteChanged(
    const rtc::NetworkRoute& network_route,
    int start_bitrate_bps,
    int min_bitrate_bps,
    int max_bitrate_bps) {
  transport_feedback_adapter_.SetNetworkIds(network_route.local_network_id,
                                            network_route.remote_network_id);

  NetworkRouteChange msg;
  msg.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  msg.constraints = ConvertConstraints(min_bitrate_bps, max_bitrate_bps,
                                       start_bitrate_bps, clock_);
  WaitOnTask([this, msg]() {
    controller_->OnNetworkRouteChange(msg);
    pacer_controller_->OnNetworkRouteChange(msg);
  });
}

bool SendSideCongestionController::AvailableBandwidth(
    uint32_t* bandwidth) const {
  // TODO(srte): Remove this interface and push information about bandwidth
  // estimation to users of this class, thereby reducing synchronous calls.
  if (control_handler->last_transfer_rate().has_value()) {
    *bandwidth =
        control_handler->last_transfer_rate()->network_estimate.bandwidth.bps();
    return true;
  }
  return false;
}

RtcpBandwidthObserver* SendSideCongestionController::GetBandwidthObserver() {
  return this;
}

RateLimiter* SendSideCongestionController::GetRetransmissionRateLimiter() {
  return control_handler->retransmission_rate_limiter();
}

void SendSideCongestionController::EnablePeriodicAlrProbing(bool enable) {
  WaitOnTask([this, enable]() {
    streams_config_.requests_alr_probing = enable;
    UpdateStreamsConfig();
  });
}

void SendSideCongestionController::UpdateStreamsConfig() {
  RTC_DCHECK(task_queue_->IsCurrent());
  streams_config_.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  controller_->OnStreamsConfig(streams_config_);
}

int64_t SendSideCongestionController::GetPacerQueuingDelayMs() const {
  // TODO(srte): This should be made less synchronous. Now it grabs a lock in
  // the pacer just for stats usage. Some kind of push interface might make
  // sense.
  return network_available_ ? pacer_->QueueInMs() : 0;
}

int64_t SendSideCongestionController::GetFirstPacketTimeMs() const {
  return pacer_->FirstSentPacketTimeMs();
}

TransportFeedbackObserver*
SendSideCongestionController::GetTransportFeedbackObserver() {
  return this;
}

void SendSideCongestionController::SignalNetworkState(NetworkState state) {
  RTC_LOG(LS_INFO) << "SignalNetworkState "
                   << (state == kNetworkUp ? "Up" : "Down");
  NetworkAvailability msg;
  msg.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  msg.network_available = state == kNetworkUp;
  network_available_ = msg.network_available;
  WaitOnTask([this, msg]() {
    controller_->OnNetworkAvailability(msg);
    pacer_controller_->OnNetworkAvailability(msg);
    control_handler->OnNetworkAvailability(msg);
  });
}

void SendSideCongestionController::SetTransportOverhead(
    size_t transport_overhead_bytes_per_packet) {
  transport_overhead_bytes_per_packet_ = transport_overhead_bytes_per_packet;
}

void SendSideCongestionController::OnSentPacket(
    const rtc::SentPacket& sent_packet) {
  // We're not interested in packets without an id, which may be stun packets,
  // etc, sent on the same transport.
  if (sent_packet.packet_id == -1)
    return;
  transport_feedback_adapter_.OnSentPacket(sent_packet.packet_id,
                                           sent_packet.send_time_ms);
  MaybeUpdateOutstandingData();
  auto packet = transport_feedback_adapter_.GetPacket(sent_packet.packet_id);
  if (packet.has_value()) {
    SentPacket msg;
    msg.size = DataSize::bytes(packet->payload_size);
    msg.send_time = Timestamp::ms(packet->send_time_ms);
    task_queue_->PostTask([this, msg]() { controller_->OnSentPacket(msg); });
  }
}

void SendSideCongestionController::OnRttUpdate(int64_t avg_rtt_ms,
                                               int64_t max_rtt_ms) {
  int64_t now_ms = clock_->TimeInMilliseconds();
  RoundTripTimeUpdate report;
  report.receive_time = Timestamp::ms(now_ms);
  report.round_trip_time = TimeDelta::ms(avg_rtt_ms);
  report.smoothed = true;
  task_queue_->PostTask(
      [this, report]() { controller_->OnRoundTripTimeUpdate(report); });
}

int64_t SendSideCongestionController::TimeUntilNextProcess() {
  const int kMaxProcessInterval = 60 * 1000;
  if (process_interval_.IsInfinite())
    return kMaxProcessInterval;
  int64_t next_process_ms = last_process_update_ms_ + process_interval_.ms();
  int64_t time_until_next_process =
      next_process_ms - clock_->TimeInMilliseconds();
  return std::max<int64_t>(time_until_next_process, 0);
}

void SendSideCongestionController::Process() {
  int64_t now_ms = clock_->TimeInMilliseconds();
  last_process_update_ms_ = now_ms;
  {
    ProcessInterval msg;
    msg.at_time = Timestamp::ms(now_ms);
    task_queue_->PostTask(
        [this, msg]() { controller_->OnProcessInterval(msg); });
  }
  if (control_handler->pacer_configured()) {
    PacerQueueUpdate msg;
    msg.expected_queue_time = TimeDelta::ms(pacer_->ExpectedQueueTimeMs());
    task_queue_->PostTask(
        [this, msg]() { control_handler->OnPacerQueueUpdate(msg); });
  }
}

void SendSideCongestionController::AddPacket(
    uint32_t ssrc,
    uint16_t sequence_number,
    size_t length,
    const PacedPacketInfo& pacing_info) {
  if (send_side_bwe_with_overhead_) {
    length += transport_overhead_bytes_per_packet_;
  }
  transport_feedback_adapter_.AddPacket(ssrc, sequence_number, length,
                                        pacing_info);
}

void SendSideCongestionController::OnTransportFeedback(
    const rtcp::TransportFeedback& feedback) {
  RTC_DCHECK_RUNS_SERIALIZED(&worker_race_);
  int64_t feedback_time_ms = clock_->TimeInMilliseconds();

  DataSize prior_in_flight =
      DataSize::bytes(transport_feedback_adapter_.GetOutstandingBytes());
  transport_feedback_adapter_.OnTransportFeedback(feedback);
  MaybeUpdateOutstandingData();

  std::vector<PacketFeedback> feedback_vector =
      transport_feedback_adapter_.GetTransportFeedbackVector();
  SortPacketFeedbackVector(&feedback_vector);

  if (!feedback_vector.empty()) {
    TransportPacketsFeedback msg;
    msg.packet_feedbacks = PacketResultsFromRtpFeedbackVector(feedback_vector);
    msg.feedback_time = Timestamp::ms(feedback_time_ms);
    msg.prior_in_flight = prior_in_flight;
    msg.data_in_flight =
        DataSize::bytes(transport_feedback_adapter_.GetOutstandingBytes());
    task_queue_->PostTask(
        [this, msg]() { controller_->OnTransportPacketsFeedback(msg); });
  }
}

void SendSideCongestionController::MaybeUpdateOutstandingData() {
  OutstandingData msg;
  msg.in_flight_data =
      DataSize::bytes(transport_feedback_adapter_.GetOutstandingBytes());
  task_queue_->PostTask(
      [this, msg]() { pacer_controller_->OnOutstandingData(msg); });
}

std::vector<PacketFeedback>
SendSideCongestionController::GetTransportFeedbackVector() const {
  RTC_DCHECK_RUNS_SERIALIZED(&worker_race_);
  return transport_feedback_adapter_.GetTransportFeedbackVector();
}

void SendSideCongestionController::WaitOnTasks() {
  rtc::Event event(false, false);
  task_queue_->PostTask([&event]() { event.Set(); });
  event.Wait(rtc::Event::kForever);
}

void SendSideCongestionController::WaitOnTask(std::function<void()> closure) {
  rtc::Event done(false, false);
  task_queue_->PostTask(rtc::NewClosure(closure, [&done] { done.Set(); }));
  done.Wait(rtc::Event::kForever);
}

void SendSideCongestionController::SetSendBitrateLimits(
    int64_t min_send_bitrate_bps,
    int64_t max_padding_bitrate_bps) {
  WaitOnTask([this, min_send_bitrate_bps, max_padding_bitrate_bps]() {
    streams_config_.min_pacing_rate = DataRate::bps(min_send_bitrate_bps);
    streams_config_.max_padding_rate = DataRate::bps(max_padding_bitrate_bps);
    UpdateStreamsConfig();
  });
}

void SendSideCongestionController::SetPacingFactor(float pacing_factor) {
  WaitOnTask([this, pacing_factor]() {
    streams_config_.pacing_factor = pacing_factor;
    UpdateStreamsConfig();
  });
}

void SendSideCongestionController::OnReceivedEstimatedBitrate(
    uint32_t bitrate) {
  RemoteBitrateReport msg;
  msg.receive_time = Timestamp::ms(clock_->TimeInMilliseconds());
  msg.bandwidth = DataRate::bps(bitrate);
  task_queue_->PostTask(
      [this, msg]() { controller_->OnRemoteBitrateReport(msg); });
}

void SendSideCongestionController::OnReceivedRtcpReceiverReport(
    const webrtc::ReportBlockList& report_blocks,
    int64_t rtt_ms,
    int64_t now_ms) {
  OnReceivedRtcpReceiverReportBlocks(report_blocks, now_ms);

  RoundTripTimeUpdate report;
  report.receive_time = Timestamp::ms(now_ms);
  report.round_trip_time = TimeDelta::ms(rtt_ms);
  report.smoothed = false;
  task_queue_->PostTask(
      [this, report]() { controller_->OnRoundTripTimeUpdate(report); });
}

void SendSideCongestionController::OnReceivedRtcpReceiverReportBlocks(
    const ReportBlockList& report_blocks,
    int64_t now_ms) {
  if (report_blocks.empty())
    return;

  int total_packets_lost_delta = 0;
  int total_packets_delta = 0;

  // Compute the packet loss from all report blocks.
  for (const RTCPReportBlock& report_block : report_blocks) {
    auto it = last_report_blocks_.find(report_block.source_ssrc);
    if (it != last_report_blocks_.end()) {
      auto number_of_packets = report_block.extended_highest_sequence_number -
                               it->second.extended_highest_sequence_number;
      total_packets_delta += number_of_packets;
      auto lost_delta = report_block.packets_lost - it->second.packets_lost;
      total_packets_lost_delta += lost_delta;
    }
    last_report_blocks_[report_block.source_ssrc] = report_block;
  }
  // Can only compute delta if there has been previous blocks to compare to. If
  // not, total_packets_delta will be unchanged and there's nothing more to do.
  if (!total_packets_delta)
    return;
  int packets_received_delta = total_packets_delta - total_packets_lost_delta;
  // To detect lost packets, at least one packet has to be received. This check
  // is needed to avoid bandwith detection update in
  // VideoSendStreamTest.SuspendBelowMinBitrate

  if (packets_received_delta < 1)
    return;
  Timestamp now = Timestamp::ms(now_ms);
  TransportLossReport msg;
  msg.packets_lost_delta = total_packets_lost_delta;
  msg.packets_received_delta = packets_received_delta;
  msg.receive_time = now;
  msg.start_time = last_report_block_time_;
  msg.end_time = now;
  task_queue_->PostTask(
      [this, msg]() { controller_->OnTransportLossReport(msg); });
  last_report_block_time_ = now;
}
}  // namespace webrtc
