/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_TEST_NETWORK_CONTROL_TESTER_H_
#define MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_TEST_NETWORK_CONTROL_TESTER_H_

#include <deque>
#include <functional>
#include <memory>

#include "api/optional.h"
#include "modules/congestion_controller/network_control/include/network_control.h"

namespace webrtc {
namespace test {
struct NetworkControlState {
  NetworkControlState();
  NetworkControlState(const NetworkControlState&);
  ~NetworkControlState();
  rtc::Optional<CongestionWindow> congestion_window;
  rtc::Optional<PacerConfig> pacer_config;
  rtc::Optional<ProbeClusterConfig> probe_config;
  rtc::Optional<TargetTransferRate> target_rate;
};

// Produces one packet per time delta
class SimpleTargetRateProducer {
 public:
  static SentPacket ProduceNext(const NetworkControlState& state,
                                Timestamp current_time,
                                TimeDelta time_delta);
};
class NetworkControlCacher : public NetworkControllerObserver {
 public:
  NetworkControlCacher();
  ~NetworkControlCacher() override;
  void OnCongestionWindow(CongestionWindow msg) override;
  void OnPacerConfig(PacerConfig msg) override;
  void OnProbeClusterConfig(ProbeClusterConfig) override;
  void OnTargetTransferRate(TargetTransferRate msg) override;
  NetworkControlState GetState() { return current_state_; }

 private:
  NetworkControlState current_state_;
};

class NetworkControllerTester {
 public:
  // A PacketProducer is a function that takes a network control state, a
  // timestamp representing the expected send time and a time delta of the send
  // times (This allows the PacketProducer to be stateless). It returns a
  // SentPacket struct with actual send time and packet size.
  using PacketProducer = std::function<
      SentPacket(const NetworkControlState&, Timestamp, TimeDelta)>;
  NetworkControllerTester(NetworkControllerFactoryInterface* factory,
                          NetworkControllerConfig initial_config);
  ~NetworkControllerTester();

  // Runs the simulations for the given duration, the PacketProducer will be
  // called repeatedly based on the given packet interval and the network will
  // be simulated using given bandwidth and propagation delay. The simulation
  // will call the controller under test with OnSentPacket and
  // OnTransportPacketsFeedback.

  // Note that OnTransportPacketsFeedback will only be called for
  // packets with resulting feedback time within the simulated duration. Packets
  // with later feedback time are saved and used in the next call to
  // RunSimulation where enough simulated time has passed.
  void RunSimulation(TimeDelta duration,
                     TimeDelta packet_interval,
                     DataRate actual_bandwidth,
                     TimeDelta propagation_delay,
                     PacketProducer next_packet);
  NetworkControlState GetState() { return cacher_.GetState(); }

 private:
  PacketResult SimulateSend(SentPacket packet,
                            TimeDelta time_delta,
                            TimeDelta propagation_delay,
                            DataRate actual_bandwidth);
  NetworkControlCacher cacher_;
  std::unique_ptr<NetworkControllerInterface> controller_;
  TimeDelta process_interval_;
  Timestamp current_time_;
  TimeDelta accumulated_delay_;
  std::deque<PacketResult> outstanding_packets_;
};
}  // namespace test
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_TEST_NETWORK_CONTROL_TESTER_H_
