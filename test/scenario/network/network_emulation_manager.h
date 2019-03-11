/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_SCENARIO_NETWORK_NETWORK_EMULATION_MANAGER_H_
#define TEST_SCENARIO_NETWORK_NETWORK_EMULATION_MANAGER_H_

#include <memory>
#include <utility>
#include <vector>

#include "api/test/simulated_network.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/logging.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/thread.h"
#include "system_wrappers/include/clock.h"
#include "test/scenario/network/cross_traffic.h"
#include "test/scenario/network/fake_network_socket_server.h"
#include "test/scenario/network/network_emulation.h"
#include "test/scenario/network/traffic_route.h"

namespace webrtc {
namespace test {

struct EndpointConfig {
  enum IpAddressFamily { kIpv4, kIpv6 };

  EndpointConfig();
  ~EndpointConfig();
  EndpointConfig(EndpointConfig&);
  EndpointConfig& operator=(EndpointConfig&);
  EndpointConfig(EndpointConfig&&);
  EndpointConfig& operator=(EndpointConfig&&);

  IpAddressFamily generated_ip_family = IpAddressFamily::kIpv4;
  // If specified will be used as IP address for endpoint node. Should be unique
  // among all created nodes.
  absl::optional<rtc::IPAddress> ip;
};

class NetworkEmulationManager {
 public:
  NetworkEmulationManager();
  ~NetworkEmulationManager();

  EmulatedNetworkNode* CreateEmulatedNode(
      std::unique_ptr<NetworkBehaviorInterface> network_behavior);

  EmulatedEndpoint* CreateEndpoint(EndpointConfig config);

  void CreateRoute(EmulatedEndpoint* from,
                   std::vector<EmulatedNetworkNode*> via_nodes,
                   EmulatedEndpoint* to);
  void ClearRoute(EmulatedEndpoint* from,
                  std::vector<EmulatedNetworkNode*> via_nodes,
                  EmulatedEndpoint* to);

  TrafficRoute* CreateTrafficRoute(std::vector<EmulatedNetworkNode*> via_nodes);
  RandomWalkCrossTraffic* CreateRandomWalkCrossTraffic(
      TrafficRoute* traffic_route,
      RandomWalkConfig config);
  PulsedPeaksCrossTraffic* CreatePulsedPeaksCrossTraffic(
      TrafficRoute* traffic_route,
      PulsedPeaksConfig config);

  rtc::Thread* CreateNetworkThread(std::vector<EmulatedEndpoint*> endpoints);

 private:
  FakeNetworkSocketServer* CreateSocketServer(
      std::vector<EmulatedEndpoint*> endpoints);
  absl::optional<rtc::IPAddress> GetNextIPv4Address();
  void ProcessNetworkPackets();
  Timestamp Now() const;

  Clock* const clock_;
  int next_node_id_;

  RepeatingTaskHandle process_task_handle_;

  uint32_t next_ip4_address_;
  std::set<rtc::IPAddress> used_ip_addresses_;

  // All objects can be added to the manager only when it is idle.
  std::vector<std::unique_ptr<EmulatedEndpoint>> endpoints_;
  std::vector<std::unique_ptr<EmulatedNetworkNode>> network_nodes_;
  std::vector<std::unique_ptr<TrafficRoute>> traffic_routes_;
  std::vector<std::unique_ptr<RandomWalkCrossTraffic>> random_cross_traffics_;
  std::vector<std::unique_ptr<PulsedPeaksCrossTraffic>> pulsed_cross_traffics_;
  std::vector<std::unique_ptr<FakeNetworkSocketServer>> socket_servers_;
  std::vector<std::unique_ptr<rtc::Thread>> threads_;

  // Must be the last field, so it will be deleted first, because tasks
  // in the TaskQueue can access other fields of the instance of this class.
  rtc::TaskQueue task_queue_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_NETWORK_NETWORK_EMULATION_MANAGER_H_
