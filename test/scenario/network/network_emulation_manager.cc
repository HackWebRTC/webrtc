/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/scenario/network/network_emulation_manager.h"

#include <algorithm>
#include <memory>

#include "absl/memory/memory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc {
namespace test {
namespace {

constexpr int64_t kPacketProcessingIntervalMs = 1;

}  // namespace

NetworkEmulationManager::NetworkEmulationManager()
    : clock_(Clock::GetRealTimeClock()),
      next_node_id_(1),
      task_queue_("network_emulation_manager") {
  process_task_handle_ = RepeatingTaskHandle::Start(&task_queue_, [this] {
    ProcessNetworkPackets();
    return TimeDelta::ms(kPacketProcessingIntervalMs);
  });
}

// TODO(srte): Ensure that any pending task that must be run for consistency
// (such as stats collection tasks) are not cancelled when the task queue is
// destroyed.
NetworkEmulationManager::~NetworkEmulationManager() = default;

EmulatedNetworkNode* NetworkEmulationManager::CreateEmulatedNode(
    std::unique_ptr<NetworkBehaviorInterface> network_behavior) {
  auto node =
      absl::make_unique<EmulatedNetworkNode>(std::move(network_behavior));
  EmulatedNetworkNode* out = node.get();

  struct Closure {
    void operator()() { manager->network_nodes_.push_back(std::move(node)); }
    NetworkEmulationManager* manager;
    std::unique_ptr<EmulatedNetworkNode> node;
  };
  task_queue_.PostTask(Closure{this, std::move(node)});
  return out;
}

EndpointNode* NetworkEmulationManager::CreateEndpoint(rtc::IPAddress ip) {
  auto node = absl::make_unique<EndpointNode>(next_node_id_++, ip, clock_);
  EndpointNode* out = node.get();
  endpoints_.push_back(std::move(node));
  return out;
}

void NetworkEmulationManager::CreateRoute(
    EndpointNode* from,
    std::vector<EmulatedNetworkNode*> via_nodes,
    EndpointNode* to) {
  // Because endpoint has no send node by default at least one should be
  // provided here.
  RTC_CHECK(!via_nodes.empty());

  from->SetSendNode(via_nodes[0]);
  EmulatedNetworkNode* cur_node = via_nodes[0];
  for (size_t i = 1; i < via_nodes.size(); ++i) {
    cur_node->SetReceiver(to->GetId(), via_nodes[i]);
    cur_node = via_nodes[i];
  }
  cur_node->SetReceiver(to->GetId(), to);
  from->SetConnectedEndpointId(to->GetId());
}

void NetworkEmulationManager::ClearRoute(
    EndpointNode* from,
    std::vector<EmulatedNetworkNode*> via_nodes,
    EndpointNode* to) {
  // Remove receiver from intermediate nodes.
  for (auto* node : via_nodes) {
    node->RemoveReceiver(to->GetId());
  }
  // Detach endpoint from current send node.
  if (from->GetSendNode()) {
    from->GetSendNode()->RemoveReceiver(to->GetId());
    from->SetSendNode(nullptr);
  }
}

TrafficRoute* NetworkEmulationManager::CreateTrafficRoute(
    std::vector<EmulatedNetworkNode*> via_nodes) {
  RTC_CHECK(!via_nodes.empty());
  EndpointNode* endpoint = CreateEndpoint(rtc::IPAddress(next_node_id_++));

  // Setup a route via specified nodes.
  EmulatedNetworkNode* cur_node = via_nodes[0];
  for (size_t i = 1; i < via_nodes.size(); ++i) {
    cur_node->SetReceiver(endpoint->GetId(), via_nodes[i]);
    cur_node = via_nodes[i];
  }
  cur_node->SetReceiver(endpoint->GetId(), endpoint);

  std::unique_ptr<TrafficRoute> traffic_route =
      absl::make_unique<TrafficRoute>(clock_, via_nodes[0], endpoint);
  TrafficRoute* out = traffic_route.get();
  traffic_routes_.push_back(std::move(traffic_route));
  return out;
}

RandomWalkCrossTraffic* NetworkEmulationManager::CreateRandomWalkCrossTraffic(
    TrafficRoute* traffic_route,
    RandomWalkConfig config) {
  auto traffic = absl::make_unique<RandomWalkCrossTraffic>(std::move(config),
                                                           traffic_route);
  RandomWalkCrossTraffic* out = traffic.get();
  struct Closure {
    void operator()() {
      manager->random_cross_traffics_.push_back(std::move(traffic));
    }
    NetworkEmulationManager* manager;
    std::unique_ptr<RandomWalkCrossTraffic> traffic;
  };
  task_queue_.PostTask(Closure{this, std::move(traffic)});
  return out;
}

PulsedPeaksCrossTraffic* NetworkEmulationManager::CreatePulsedPeaksCrossTraffic(
    TrafficRoute* traffic_route,
    PulsedPeaksConfig config) {
  auto traffic = absl::make_unique<PulsedPeaksCrossTraffic>(std::move(config),
                                                            traffic_route);
  PulsedPeaksCrossTraffic* out = traffic.get();
  struct Closure {
    void operator()() {
      manager->pulsed_cross_traffics_.push_back(std::move(traffic));
    }
    NetworkEmulationManager* manager;
    std::unique_ptr<PulsedPeaksCrossTraffic> traffic;
  };
  task_queue_.PostTask(Closure{this, std::move(traffic)});
  return out;
}

rtc::Thread* NetworkEmulationManager::CreateNetworkThread(
    std::vector<EndpointNode*> endpoints) {
  FakeNetworkSocketServer* socket_server = CreateSocketServer(endpoints);
  std::unique_ptr<rtc::Thread> network_thread =
      absl::make_unique<rtc::Thread>(socket_server);
  network_thread->SetName("network_thread" + std::to_string(threads_.size()),
                          nullptr);
  network_thread->Start();
  rtc::Thread* out = network_thread.get();
  threads_.push_back(std::move(network_thread));
  return out;
}

FakeNetworkSocketServer* NetworkEmulationManager::CreateSocketServer(
    std::vector<EndpointNode*> endpoints) {
  auto socket_server =
      absl::make_unique<FakeNetworkSocketServer>(clock_, endpoints);
  FakeNetworkSocketServer* out = socket_server.get();
  socket_servers_.push_back(std::move(socket_server));
  return out;
}

void NetworkEmulationManager::ProcessNetworkPackets() {
  Timestamp current_time = Now();
  for (auto& traffic : random_cross_traffics_) {
    traffic->Process(current_time);
  }
  for (auto& traffic : pulsed_cross_traffics_) {
    traffic->Process(current_time);
  }
  for (auto& node : network_nodes_) {
    node->Process(current_time);
  }
}

Timestamp NetworkEmulationManager::Now() const {
  return Timestamp::us(clock_->TimeInMicroseconds());
}

}  // namespace test
}  // namespace webrtc
