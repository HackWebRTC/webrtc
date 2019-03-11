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
// uint32_t representation of 192.168.0.0 address
constexpr uint32_t kMinIPv4Address = 0xC0A80000;
// uint32_t representation of 192.168.255.255 address
constexpr uint32_t kMaxIPv4Address = 0xC0A8FFFF;

}  // namespace

EndpointConfig::EndpointConfig() = default;
EndpointConfig::~EndpointConfig() = default;
EndpointConfig::EndpointConfig(EndpointConfig&) = default;
EndpointConfig& EndpointConfig::operator=(EndpointConfig&) = default;
EndpointConfig::EndpointConfig(EndpointConfig&&) = default;
EndpointConfig& EndpointConfig::operator=(EndpointConfig&&) = default;

NetworkEmulationManager::NetworkEmulationManager()
    : clock_(Clock::GetRealTimeClock()),
      next_node_id_(1),
      next_ip4_address_(kMinIPv4Address),
      task_queue_("network_emulation_manager") {
  process_task_handle_ = RepeatingTaskHandle::Start(task_queue_.Get(), [this] {
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

EmulatedEndpoint* NetworkEmulationManager::CreateEndpoint(
    EndpointConfig config) {
  absl::optional<rtc::IPAddress> ip = config.ip;
  if (!ip) {
    switch (config.generated_ip_family) {
      case EndpointConfig::IpAddressFamily::kIpv4:
        ip = GetNextIPv4Address();
        RTC_CHECK(ip) << "All auto generated IPv4 addresses exhausted";
        break;
      case EndpointConfig::IpAddressFamily::kIpv6:
        ip = GetNextIPv4Address();
        RTC_CHECK(ip) << "All auto generated IPv6 addresses exhausted";
        ip = ip->AsIPv6Address();
        break;
    }
  }

  bool res = used_ip_addresses_.insert(*ip).second;
  RTC_CHECK(res) << "IP=" << ip->ToString() << " already in use";
  auto node = absl::make_unique<EmulatedEndpoint>(next_node_id_++, *ip, clock_);
  EmulatedEndpoint* out = node.get();
  endpoints_.push_back(std::move(node));
  return out;
}

void NetworkEmulationManager::CreateRoute(
    EmulatedEndpoint* from,
    std::vector<EmulatedNetworkNode*> via_nodes,
    EmulatedEndpoint* to) {
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
    EmulatedEndpoint* from,
    std::vector<EmulatedNetworkNode*> via_nodes,
    EmulatedEndpoint* to) {
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
  EmulatedEndpoint* endpoint = CreateEndpoint(EndpointConfig());

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
    std::vector<EmulatedEndpoint*> endpoints) {
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
    std::vector<EmulatedEndpoint*> endpoints) {
  auto socket_server =
      absl::make_unique<FakeNetworkSocketServer>(clock_, endpoints);
  FakeNetworkSocketServer* out = socket_server.get();
  socket_servers_.push_back(std::move(socket_server));
  return out;
}

absl::optional<rtc::IPAddress> NetworkEmulationManager::GetNextIPv4Address() {
  uint32_t addresses_count = kMaxIPv4Address - kMinIPv4Address;
  for (uint32_t i = 0; i < addresses_count; i++) {
    rtc::IPAddress ip(next_ip4_address_);
    if (next_ip4_address_ == kMaxIPv4Address) {
      next_ip4_address_ = kMinIPv4Address;
    } else {
      next_ip4_address_++;
    }
    if (used_ip_addresses_.find(ip) == used_ip_addresses_.end()) {
      return ip;
    }
  }
  return absl::nullopt;
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
