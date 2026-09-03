/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "modules/roq/receive_flow_registry.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace webrtc::roq {

ReceiveFlowRegistry::ReceiveFlowRegistry(std::vector<uint64_t> flow_ids)
    {
  for (uint64_t flow_id : flow_ids)
    flows_[flow_id] = ReceiveFlowPolicy::kDeliver;
}

void ReceiveFlowRegistry::BeginUpdate(Timestamp now) {
  dropped_count_ += pending_.size();
  pending_.clear();
  pending_bytes_ = 0;
  update_active_ = true;
  update_deadline_ = now + kGraceDuration;
}

ReceiveFlowRoute ReceiveFlowRegistry::Route(const RoqReceivedPacket& packet,
                                            Timestamp now) {
  if (auto flow = flows_.find(packet.flow_id); flow != flows_.end()) {
    if (flow->second == ReceiveFlowPolicy::kDeliver)
      return ReceiveFlowRoute::kDeliver;
    ++dropped_count_;
    return ReceiveFlowRoute::kDiscard;
  }
  if (!update_active_ || now > update_deadline_) {
    dropped_count_ += pending_.size() + 1;
    pending_.clear();
    pending_bytes_ = 0;
    update_active_ = false;
    return RecordUnknown(packet, now);
  }
  if (packet.packet.size() > kMaximumBytes) {
    ++dropped_count_;
    return ReceiveFlowRoute::kBuffered;
  }
  size_t packets_for_flow = std::count_if(
      pending_.begin(), pending_.end(), [&](const BufferedPacket& buffered) {
        return buffered.packet.flow_id == packet.flow_id;
      });
  while (packets_for_flow >= kMaximumPacketsPerFlow) {
    DropOldestForFlow(packet.flow_id);
    --packets_for_flow;
  }
  while (!pending_.empty() &&
         (pending_.size() >= kMaximumPackets ||
          pending_bytes_ + packet.packet.size() > kMaximumBytes)) {
    DropOldest();
  }
  pending_bytes_ += packet.packet.size();
  pending_.push_back({packet, packet.packet.size()});
  ++buffered_count_;
  return ReceiveFlowRoute::kBuffered;
}

std::vector<RoqReceivedPacket> ReceiveFlowRegistry::Update(
    std::vector<uint64_t> flow_ids) {
  std::vector<ReceiveFlowConfig> configs;
  configs.reserve(flow_ids.size());
  for (uint64_t flow_id : flow_ids)
    configs.push_back({flow_id, ReceiveFlowPolicy::kDeliver});
  return Update(std::move(configs));
}

std::vector<RoqReceivedPacket> ReceiveFlowRegistry::Update(
    std::vector<ReceiveFlowConfig> flow_configs) {
  flows_.clear();
  for (const ReceiveFlowConfig& config : flow_configs)
    flows_[config.flow_id] = config.policy;
  update_active_ = false;
  std::vector<RoqReceivedPacket> released;
  for (BufferedPacket& buffered : pending_) {
    auto flow = flows_.find(buffered.packet.flow_id);
    if (flow != flows_.end() && flow->second == ReceiveFlowPolicy::kDeliver) {
      released.push_back(std::move(buffered.packet));
      ++released_count_;
    } else {
      ++dropped_count_;
    }
  }
  pending_.clear();
  pending_bytes_ = 0;
  return released;
}

bool ReceiveFlowRegistry::IsKnown(uint64_t flow_id) const {
  return flows_.contains(flow_id);
}

void ReceiveFlowRegistry::DropOldest() {
  pending_bytes_ -= pending_.front().bytes;
  pending_.pop_front();
  ++dropped_count_;
}

void ReceiveFlowRegistry::DropOldestForFlow(uint64_t flow_id) {
  auto oldest = std::find_if(
      pending_.begin(), pending_.end(), [&](const BufferedPacket& buffered) {
        return buffered.packet.flow_id == flow_id;
      });
  if (oldest == pending_.end())
    return;
  pending_bytes_ -= oldest->bytes;
  pending_.erase(oldest);
  ++dropped_count_;
}

ReceiveFlowRoute ReceiveFlowRegistry::RecordUnknown(
    const RoqReceivedPacket& packet,
    Timestamp now) {
  if (unknown_window_start_.IsInfinite() ||
      now - unknown_window_start_ >= kUnknownWindow) {
    unknown_window_start_ = now;
    unknown_packets_ = 0;
    unknown_bytes_ = 0;
  }
  ++unknown_packets_;
  unknown_bytes_ += packet.packet.size();
  return unknown_packets_ >= kUnknownMaximumPackets ||
                 unknown_bytes_ >= kUnknownMaximumBytes
             ? ReceiveFlowRoute::kClose
             : ReceiveFlowRoute::kDiscard;
}

}  // namespace webrtc::roq
