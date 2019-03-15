/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_NETWORK_EMULATION_MANAGER_H_
#define API_TEST_NETWORK_EMULATION_MANAGER_H_

#include <memory>
#include <vector>

#include "api/test/simulated_network.h"
#include "rtc_base/network.h"
#include "rtc_base/thread.h"

namespace webrtc {

// This API is still in development and can be changed without prior notice.

// These classes are forward declared here, because they used as handles, to
// make it possible for client code to operate with these abstractions and build
// required network configuration. With forward declaration here implementation
// is more readable, than with interfaces approach and cause user needn't any
// API methods on these abstractions it is acceptable here.

// EmulatedNetworkNode is an abstraction for some network in the real world,
// like 3G network between peers, or Wi-Fi for one peer and LTE for another.
// Multiple networks can be joined into chain emulating a network path from
// one peer to another.
class EmulatedNetworkNode;
// EmulatedEndpoint is and abstraction for network interface on device.
class EmulatedEndpoint;
// EmulatedRoute is handle for single route from one network interface on one
// peer device to another network interface on another peer device.
class EmulatedRoute;

struct EmulatedEndpointConfig {
  enum class IpAddressFamily { kIpv4, kIpv6 };

  IpAddressFamily generated_ip_family = IpAddressFamily::kIpv4;
  // If specified will be used as IP address for endpoint node. Must be unique
  // among all created nodes.
  absl::optional<rtc::IPAddress> ip;
};

// Provides an API for creating and configuring emulated network layer.
// All objects returned by this API are owned by NetworkEmulationManager itself
// and will be deleted when manager will be deleted.
class NetworkEmulationManager {
 public:
  virtual ~NetworkEmulationManager() = default;

  // Creates an emulated network node, which represents single network in
  // the emulated network layer.
  virtual EmulatedNetworkNode* CreateEmulatedNode(
      std::unique_ptr<NetworkBehaviorInterface> network_behavior) = 0;

  // Creates an emulated endpoint, which represents single network interface on
  // the peer's device.
  virtual EmulatedEndpoint* CreateEndpoint(EmulatedEndpointConfig config) = 0;

  // Creates a route between endpoints going through specified network nodes.
  // This route is single direction only and describe how traffic that was
  // sent by network interface |from| have to be delivered to the network
  // interface |to|. Return object can be used to remove created route.
  //
  // Assume there are endpoints E1, E2 and E3 and network nodes A, B, C and D.
  // Also assume, that there is a route constructed via A, B and C like this:
  // E1 -> A -> B -> C -> E2. In such case:
  //   * Caller mustn't use A, B and C in any route, that is leading to E2.
  //   * If caller will then create a new route E1 -> D -> E3, then first
  //     route will be corrupted, so if caller want to do this, first route
  //     should be deleted by ClearRoute(...) and then a new one should be
  //     created.
  //   * Caller can use A, B or C for any other routes.
  //   * Caller can create other routes leading to E2.
  virtual EmulatedRoute* CreateRoute(
      EmulatedEndpoint* from,
      const std::vector<EmulatedNetworkNode*>& via_nodes,
      EmulatedEndpoint* to) = 0;
  // Removes route previously created by CreateRoute(...).
  // Caller mustn't call this function with route, that have been already
  // removed earlier.
  virtual void ClearRoute(EmulatedRoute* route) = 0;

  // Creates rtc::Thread that should be used as network thread for peer
  // connection. Created thread contains special rtc::SocketServer inside it
  // to enable correct integration between peer connection and emulated network
  // layer.
  virtual rtc::Thread* CreateNetworkThread(
      const std::vector<EmulatedEndpoint*>& endpoints) = 0;
  // Creates rtc::NetworkManager that should be used inside
  // cricket::PortAllocator for peer connection to provide correct list of
  // network interfaces, that exists in emulated network later.
  virtual rtc::NetworkManager* CreateNetworkManager(
      const std::vector<EmulatedEndpoint*>& endpoints) = 0;
};

}  // namespace webrtc

#endif  // API_TEST_NETWORK_EMULATION_MANAGER_H_
