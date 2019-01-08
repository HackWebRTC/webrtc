/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/scenario/network/network_emulation.h"

#include <memory>

#include "absl/memory/memory.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace test {

EmulatedIpPacket::EmulatedIpPacket(const rtc::SocketAddress& from,
                                   const rtc::SocketAddress& to,
                                   uint64_t dest_endpoint_id,
                                   rtc::CopyOnWriteBuffer data,
                                   Timestamp arrival_time)
    : from(from),
      to(to),
      dest_endpoint_id(dest_endpoint_id),
      data(data),
      arrival_time(arrival_time) {}

EmulatedIpPacket::~EmulatedIpPacket() = default;

EmulatedIpPacket::EmulatedIpPacket(EmulatedIpPacket&&) = default;

void EmulatedNetworkNode::CreateRoute(
    uint64_t receiver_id,
    std::vector<EmulatedNetworkNode*> nodes,
    EmulatedNetworkReceiverInterface* receiver) {
  RTC_CHECK(!nodes.empty());
  for (size_t i = 0; i + 1 < nodes.size(); ++i)
    nodes[i]->SetReceiver(receiver_id, nodes[i + 1]);
  nodes.back()->SetReceiver(receiver_id, receiver);
}

void EmulatedNetworkNode::ClearRoute(uint64_t receiver_id,
                                     std::vector<EmulatedNetworkNode*> nodes) {
  for (EmulatedNetworkNode* node : nodes)
    node->RemoveReceiver(receiver_id);
}

EmulatedNetworkNode::EmulatedNetworkNode(
    std::unique_ptr<NetworkBehaviorInterface> network_behavior,
    size_t packet_overhead)
    : network_behavior_(std::move(network_behavior)),
      packet_overhead_(packet_overhead) {}

EmulatedNetworkNode::~EmulatedNetworkNode() = default;

void EmulatedNetworkNode::OnPacketReceived(EmulatedIpPacket packet) {
  rtc::CritScope crit(&lock_);
  if (routing_.find(packet.dest_endpoint_id) == routing_.end())
    return;
  uint64_t packet_id = next_packet_id_++;
  bool sent = network_behavior_->EnqueuePacket(PacketInFlightInfo(
      packet.size() + packet_overhead_, packet.arrival_time.us(), packet_id));
  if (sent) {
    packets_.emplace_back(StoredPacket{packet_id, std::move(packet), false});
  }
}

void EmulatedNetworkNode::Process(Timestamp at_time) {
  std::vector<PacketDeliveryInfo> delivery_infos;
  {
    rtc::CritScope crit(&lock_);
    absl::optional<int64_t> delivery_us =
        network_behavior_->NextDeliveryTimeUs();
    if (delivery_us && *delivery_us > at_time.us())
      return;

    delivery_infos = network_behavior_->DequeueDeliverablePackets(at_time.us());
  }
  for (PacketDeliveryInfo& delivery_info : delivery_infos) {
    StoredPacket* packet = nullptr;
    EmulatedNetworkReceiverInterface* receiver = nullptr;
    {
      rtc::CritScope crit(&lock_);
      for (auto& stored_packet : packets_) {
        if (stored_packet.id == delivery_info.packet_id) {
          packet = &stored_packet;
          break;
        }
      }
      RTC_CHECK(packet);
      RTC_DCHECK(!packet->removed);
      receiver = routing_[packet->packet.dest_endpoint_id];
      packet->removed = true;
    }
    RTC_CHECK(receiver);
    // We don't want to keep the lock here. Otherwise we would get a deadlock if
    // the receiver tries to push a new packet.
    packet->packet.arrival_time = Timestamp::us(delivery_info.receive_time_us);
    receiver->OnPacketReceived(std::move(packet->packet));
    {
      rtc::CritScope crit(&lock_);
      while (!packets_.empty() && packets_.front().removed) {
        packets_.pop_front();
      }
    }
  }
}

void EmulatedNetworkNode::SetReceiver(
    uint64_t dest_endpoint_id,
    EmulatedNetworkReceiverInterface* receiver) {
  rtc::CritScope crit(&lock_);
  RTC_CHECK(routing_
                .insert(std::pair<uint64_t, EmulatedNetworkReceiverInterface*>(
                    dest_endpoint_id, receiver))
                .second)
      << "Such routing already exists";
}

void EmulatedNetworkNode::RemoveReceiver(uint64_t dest_endpoint_id) {
  rtc::CritScope crit(&lock_);
  routing_.erase(dest_endpoint_id);
}

}  // namespace test
}  // namespace webrtc
