/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "api/roq/fixed_rate_network_controller.h"

#include <memory>

#include "api/transport/network_control.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc::roq {
namespace {

class FixedRateNetworkController final : public NetworkControllerInterface {
 public:
  explicit FixedRateNetworkController(DataRate media_rate)
      : media_rate_(media_rate) {}

  NetworkControlUpdate OnNetworkAvailability(NetworkAvailability msg) override {
    return Update(msg.at_time);
  }
  NetworkControlUpdate OnNetworkRouteChange(NetworkRouteChange msg) override {
    return Update(msg.at_time);
  }
  NetworkControlUpdate OnProcessInterval(ProcessInterval msg) override {
    return Update(msg.at_time);
  }
  NetworkControlUpdate OnRemoteBitrateReport(RemoteBitrateReport msg) override {
    return Update(msg.receive_time);
  }
  NetworkControlUpdate OnRoundTripTimeUpdate(RoundTripTimeUpdate msg) override {
    return Update(msg.receive_time);
  }
  NetworkControlUpdate OnSentPacket(SentPacket msg) override {
    return Update(msg.send_time);
  }
  NetworkControlUpdate OnReceivedPacket(ReceivedPacket msg) override {
    return Update(msg.receive_time);
  }
  NetworkControlUpdate OnStreamsConfig(StreamsConfig msg) override {
    return Update(msg.at_time);
  }
  NetworkControlUpdate OnTargetRateConstraints(
      TargetRateConstraints msg) override {
    return Update(msg.at_time);
  }
  NetworkControlUpdate OnTransportLossReport(TransportLossReport msg) override {
    return Update(msg.receive_time);
  }
  NetworkControlUpdate OnTransportPacketsFeedback(
      TransportPacketsFeedback msg) override {
    return Update(msg.feedback_time);
  }
  NetworkControlUpdate OnNetworkStateEstimate(
      NetworkStateEstimate msg) override {
    return Update(msg.update_time);
  }

 private:
  NetworkControlUpdate Update(Timestamp at_time) const {
    NetworkControlUpdate update;
    TargetTransferRate target;
    target.at_time = at_time;
    target.network_estimate.at_time = at_time;
    target.target_rate = media_rate_;
    target.is_bandwidth_limited = false;
    update.target_rate = target;
    update.pacer_config = PacerConfig::Create(
        at_time, media_rate_, DataRate::Zero(), TimeDelta::Millis(40));
    return update;
  }

  const DataRate media_rate_;
};

}  // namespace

FixedRateNetworkControllerFactory::FixedRateNetworkControllerFactory(
    DataRate media_rate)
    : media_rate_(media_rate) {}

std::unique_ptr<NetworkControllerInterface>
FixedRateNetworkControllerFactory::Create(NetworkControllerConfig) {
  return std::make_unique<FixedRateNetworkController>(media_rate_);
}

TimeDelta FixedRateNetworkControllerFactory::GetProcessInterval() const {
  return TimeDelta::Millis(100);
}

}  // namespace webrtc::roq
