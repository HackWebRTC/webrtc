/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "api/roq/quic_network_controller.h"

#include <memory>
#include <utility>

#include "api/transport/network_control.h"
#include "api/transport/network_types.h"
#include "modules/roq/quic_bandwidth_policy.h"

namespace webrtc::roq {
namespace {

class QuicNetworkController final : public NetworkControllerInterface {
 public:
  explicit QuicNetworkController(
      QuicNetworkControllerFactory::StatsProvider stats_provider,
      QuicNetworkControllerFactory::UpdateObserver update_observer)
      : stats_provider_(std::move(stats_provider)),
        update_observer_(std::move(update_observer)) {}

  NetworkControlUpdate OnNetworkAvailability(NetworkAvailability msg) override;
  NetworkControlUpdate OnNetworkRouteChange(NetworkRouteChange msg) override;
  NetworkControlUpdate OnProcessInterval(ProcessInterval msg) override;
  NetworkControlUpdate OnRemoteBitrateReport(RemoteBitrateReport msg) override;
  NetworkControlUpdate OnRoundTripTimeUpdate(RoundTripTimeUpdate msg) override;
  NetworkControlUpdate OnSentPacket(SentPacket msg) override;
  NetworkControlUpdate OnReceivedPacket(ReceivedPacket msg) override;
  NetworkControlUpdate OnStreamsConfig(StreamsConfig msg) override;
  NetworkControlUpdate OnTargetRateConstraints(
      TargetRateConstraints msg) override;
  NetworkControlUpdate OnTransportLossReport(TransportLossReport msg) override;
  NetworkControlUpdate OnTransportPacketsFeedback(
      TransportPacketsFeedback msg) override;
  NetworkControlUpdate OnNetworkStateEstimate(
      NetworkStateEstimate msg) override;

 private:
  NetworkControlUpdate Update(Timestamp at_time) const;

  const QuicNetworkControllerFactory::StatsProvider stats_provider_;
  const QuicNetworkControllerFactory::UpdateObserver update_observer_;
  QuicBandwidthPolicy policy_;
  QuicMediaBudget budget_;
  float loss_rate_ratio_ = 0;
};

NetworkControlUpdate QuicNetworkController::Update(Timestamp at_time) const {
  NetworkControlUpdate update;
  update.pacer_config = PacerConfig::Create(
      at_time, budget_.target_rate, DataRate::Zero(), TimeDelta::Millis(40));
  const DataRate applied_rate = update.pacer_config->data_rate();
  TargetTransferRate target;
  target.at_time = at_time;
  target.network_estimate.at_time = at_time;
  target.network_estimate.bandwidth = applied_rate;
  target.network_estimate.round_trip_time = budget_.round_trip_time;
  target.network_estimate.loss_rate_ratio = loss_rate_ratio_;
  target.target_rate = applied_rate;
  target.is_bandwidth_limited = true;
  update.target_rate = target;
  return update;
}

NetworkControlUpdate QuicNetworkController::OnNetworkAvailability(
    NetworkAvailability msg) {
  return Update(msg.at_time);
}

NetworkControlUpdate QuicNetworkController::OnNetworkRouteChange(
    NetworkRouteChange msg) {
  return Update(msg.at_time);
}

NetworkControlUpdate QuicNetworkController::OnProcessInterval(
    ProcessInterval msg) {
  const RoqSessionStats stats = stats_provider_();
  const QuicBandwidthSample sample = {
      .at_time = msg.at_time,
      .acked_bytes = stats.acked_bytes,
      .startup_complete = stats.startup_exit_cwnd > 0,
      .max_bandwidth =
          DataRate::BytesPerSec(stats.max_bandwidth_bytes_per_second),
      .delivery_rate =
          DataRate::BytesPerSec(stats.delivery_rate_bytes_per_second),
      .round_trip_time = TimeDelta::Micros(stats.rtt_ns / 1000)};
  budget_ = policy_.Update(sample);
  NetworkControlUpdate update = Update(msg.at_time);
  if (update_observer_) {
    const PacerConfig& pacer = *update.pacer_config;
    update_observer_({
        .at_time = msg.at_time,
        .acked_bytes = stats.acked_bytes,
        .startup_complete = stats.startup_exit_cwnd > 0,
        .delivery_rate_bytes_per_second = stats.delivery_rate_bytes_per_second,
        .max_bandwidth_bytes_per_second = stats.max_bandwidth_bytes_per_second,
        .round_trip_time = sample.round_trip_time,
        .fresh = budget_.fresh,
        .raw_capacity = budget_.raw_capacity,
        .overhead = budget_.overhead,
        .recovery_reserve = budget_.recovery_reserve,
        .safety_headroom = budget_.safety_headroom,
        .usable_media_rate = budget_.usable_media_rate,
        .target_rate = update.target_rate->target_rate,
        .pacer_rate = pacer.data_rate(),
        .padding_rate = pacer.pad_rate(),
        .probe_cluster_count = update.probe_cluster_configs.size(),
        .has_congestion_window = update.congestion_window.has_value(),
    });
  }
  return update;
}

NetworkControlUpdate QuicNetworkController::OnRemoteBitrateReport(
    RemoteBitrateReport msg) {
  return Update(msg.receive_time);
}

NetworkControlUpdate QuicNetworkController::OnRoundTripTimeUpdate(
    RoundTripTimeUpdate msg) {
  return Update(msg.receive_time);
}

NetworkControlUpdate QuicNetworkController::OnSentPacket(SentPacket msg) {
  return Update(msg.send_time);
}

NetworkControlUpdate QuicNetworkController::OnReceivedPacket(
    ReceivedPacket msg) {
  return Update(msg.receive_time);
}

NetworkControlUpdate QuicNetworkController::OnStreamsConfig(StreamsConfig msg) {
  return Update(msg.at_time);
}

NetworkControlUpdate QuicNetworkController::OnTargetRateConstraints(
    TargetRateConstraints msg) {
  return Update(msg.at_time);
}

NetworkControlUpdate QuicNetworkController::OnTransportLossReport(
    TransportLossReport msg) {
  const uint64_t total = msg.packets_lost_delta + msg.packets_received_delta;
  loss_rate_ratio_ =
      total == 0 ? 0.0f : static_cast<float>(msg.packets_lost_delta) / total;
  return Update(msg.receive_time);
}

NetworkControlUpdate QuicNetworkController::OnTransportPacketsFeedback(
    TransportPacketsFeedback msg) {
  return Update(msg.feedback_time);
}

NetworkControlUpdate QuicNetworkController::OnNetworkStateEstimate(
    NetworkStateEstimate msg) {
  return Update(msg.update_time);
}

}  // namespace

QuicNetworkControllerFactory::QuicNetworkControllerFactory(
    StatsProvider stats_provider,
    UpdateObserver update_observer)
    : stats_provider_(std::move(stats_provider)),
      update_observer_(std::move(update_observer)) {}

std::unique_ptr<NetworkControllerInterface>
QuicNetworkControllerFactory::Create(NetworkControllerConfig) {
  return std::make_unique<QuicNetworkController>(stats_provider_,
                                                 update_observer_);
}

TimeDelta QuicNetworkControllerFactory::GetProcessInterval() const {
  return TimeDelta::Millis(100);
}

}  // namespace webrtc::roq
