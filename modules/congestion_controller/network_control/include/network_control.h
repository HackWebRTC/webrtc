/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_INCLUDE_NETWORK_CONTROL_H_
#define MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_INCLUDE_NETWORK_CONTROL_H_
#include <stdint.h>
#include <memory>

#include "modules/congestion_controller/network_control/include/network_types.h"
#include "modules/congestion_controller/network_control/include/network_units.h"

namespace webrtc {

// NetworkControllerObserver is an interface implemented by observers of network
// controllers. It contains declarations of the possible configuration messages
// that can be sent from a network controller implementation.
class NetworkControllerObserver {
 public:
  // Called when congestion window configutation is changed.
  virtual void OnCongestionWindow(CongestionWindow) = 0;
  // Called when pacer configuration has changed.
  virtual void OnPacerConfig(PacerConfig) = 0;
  // Called to indicate that a new probe should be sent.
  virtual void OnProbeClusterConfig(ProbeClusterConfig) = 0;
  // Called to indicate target transfer rate as well as giving information about
  // the current estimate of network parameters.
  virtual void OnTargetTransferRate(TargetTransferRate) = 0;

 protected:
  virtual ~NetworkControllerObserver() = default;
};

// NetworkControllerInterface is implemented by network controllers. A network
// controller is a class that uses information about network state and traffic
// to estimate network parameters such as round trip time and bandwidth. Network
// controllers does not guarantee thread safety, the interface must be used in a
// non-concurrent fashion.
class NetworkControllerInterface {
 public:
  using uptr = std::unique_ptr<NetworkControllerInterface>;
  virtual ~NetworkControllerInterface() = default;

  // Called when network availabilty changes.
  virtual void OnNetworkAvailability(NetworkAvailability) = 0;
  // Called when the receiving or sending endpoint changes address.
  virtual void OnNetworkRouteChange(NetworkRouteChange) = 0;
  // Called periodically with a periodicy as specified by
  // NetworkControllerFactoryInterface::GetProcessInterval.
  virtual void OnProcessInterval(ProcessInterval) = 0;
  // Called when remotely calculated bitrate is received.
  virtual void OnRemoteBitrateReport(RemoteBitrateReport) = 0;
  // Called round trip time has been calculated by protocol specific mechanisms.
  virtual void OnRoundTripTimeUpdate(RoundTripTimeUpdate) = 0;
  // Called when a packet is sent on the network.
  virtual void OnSentPacket(SentPacket) = 0;
  // Called when the stream specific configuration has been updated.
  virtual void OnStreamsConfig(StreamsConfig) = 0;
  // Called when target transfer rate constraints has been changed.
  virtual void OnTargetRateConstraints(TargetRateConstraints) = 0;
  // Called when a protocol specific calculation of packet loss has been made.
  virtual void OnTransportLossReport(TransportLossReport) = 0;
  // Called with per packet feedback regarding receive time.
  virtual void OnTransportPacketsFeedback(TransportPacketsFeedback) = 0;
};

// NetworkControllerFactoryInterface is an interface for creating a network
// controller.
class NetworkControllerFactoryInterface {
 public:
  using uptr = std::unique_ptr<NetworkControllerFactoryInterface>;
  // Used to create a new network controller, requires an observer to be
  // provided to handle callbacks.
  virtual NetworkControllerInterface::uptr Create(
      NetworkControllerObserver* observer) = 0;
  // Returns the interval by which the network controller expects
  // OnProcessInterval calls.
  virtual TimeDelta GetProcessInterval() const = 0;
  virtual ~NetworkControllerFactoryInterface() = default;
};
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_INCLUDE_NETWORK_CONTROL_H_
