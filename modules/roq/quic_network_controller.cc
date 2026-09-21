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
      QuicNetworkControllerFactory::UpdateObserver update_observer,
      DataRate minimum_rate,
      DataRate startup_rate,
      DataRate maximum_rate)
      : stats_provider_(std::move(stats_provider)),
        update_observer_(std::move(update_observer)),
        policy_(minimum_rate, startup_rate, maximum_rate) {}

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
  // PacerConfig quantizes to whole bytes per 40 ms. The media target must not
  // inherit that rounding: a fast resume exactly at the native threshold can
  // otherwise arrive below it. quiche, not this metadata, owns wire pacing.
  const DataRate applied_rate = budget_.target_rate;
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
      .connection_id = stats.source_connection_id,
      .acked_bytes = stats.acked_bytes,
      .startup_complete = stats.startup_exit_cwnd > 0,
      .max_bandwidth =
          DataRate::BytesPerSec(stats.max_bandwidth_bytes_per_second),
      .delivery_rate =
          DataRate::BytesPerSec(stats.delivery_rate_bytes_per_second),
      .round_trip_time = TimeDelta::Micros(stats.rtt_ns / 1000),
      .audio_payload_rate =
          DataRate::BitsPerSec(stats.configured_audio_bitrate_bps),
      .maximum_media_rate =
          DataRate::BitsPerSec(stats.configured_max_media_bitrate_bps),
      .resume_media_rate =
          DataRate::BitsPerSec(stats.configured_resume_media_bitrate_bps),
      .evidence_round = stats.bandwidth_evidence_round,
      .evidence_rate = DataRate::BytesPerSec(stats.bandwidth_evidence_bytes_per_second),
      .evidence_age = stats.bandwidth_evidence_age_ns == UINT64_MAX
                          ? TimeDelta::PlusInfinity()
                          : TimeDelta::Micros(stats.bandwidth_evidence_age_ns / 1000),
      .audio_payload_bytes = stats.audio_payload_bytes,
      .audio_wire_bytes = stats.audio_wire_bytes,
      .audio_packets = stats.audio_packets,
      .audio_redundant_payload_bytes = stats.audio_redundant_payload_bytes,
      .audio_redundant_blocks = stats.audio_redundant_blocks,
      .audio_packet_rate = stats.configured_audio_packet_rate,
      .video_payload_bytes = stats.video_payload_bytes,
      .video_wire_bytes = stats.video_wire_bytes,
      .rtcp_wire_bytes = stats.rtcp_wire_bytes,
      .oldest_video_queue_age =
          TimeDelta::Micros(stats.oldest_video_queue_age_us),
      .expired_video_frames = stats.expired_video_frames};
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
        .capacity_known = budget_.capacity_known,
        .capacity_state = budget_.capacity_state,
        .audio_redundancy_multiplier = budget_.audio_redundancy_multiplier,
        .audio_header_rate = budget_.audio_header_rate,
        .raw_capacity = budget_.raw_capacity,
        .overhead = budget_.overhead,
        .recovery_reserve = budget_.recovery_reserve,
        .safety_headroom = budget_.safety_headroom,
        .probe_max_rate = budget_.probe_max_rate,
        .maintenance_rate = budget_.maintenance_rate,
        .resume_fast_path = budget_.resume_fast_path,
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
  policy_.SetLimits(msg.min_data_rate.value_or(DataRate::BitsPerSec(1)),
                    msg.starting_rate.value_or(budget_.target_rate),
                    msg.max_data_rate.value_or(DataRate::PlusInfinity()));
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
    UpdateObserver update_observer,
    DataRate minimum_rate,
    DataRate startup_rate,
    DataRate maximum_rate)
    : stats_provider_(std::move(stats_provider)),
      update_observer_(std::move(update_observer)),
      minimum_rate_(minimum_rate),
      startup_rate_(startup_rate),
      maximum_rate_(maximum_rate) {}

std::unique_ptr<NetworkControllerInterface>
QuicNetworkControllerFactory::Create(NetworkControllerConfig) {
  return std::make_unique<QuicNetworkController>(
      stats_provider_, update_observer_, minimum_rate_, startup_rate_,
      maximum_rate_);
}

TimeDelta QuicNetworkControllerFactory::GetProcessInterval() const {
  return TimeDelta::Millis(100);
}

}  // namespace webrtc::roq
