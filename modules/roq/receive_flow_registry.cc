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
    : flow_ids_(std::move(flow_ids)) {}

void ReceiveFlowRegistry::BeginUpdate(Timestamp now) {
  dropped_count_ += pending_.size();
  pending_.clear();
  pending_bytes_ = 0;
  update_active_ = true;
  update_deadline_ = now + kGraceDuration;
}

ReceiveFlowRoute ReceiveFlowRegistry::Route(const RoqReceivedPacket& packet,
                                            Timestamp now) {
  if (IsKnown(packet.flow_id))
    return ReceiveFlowRoute::kDeliver;
  if (!update_active_ || now > update_deadline_) {
    dropped_count_ += pending_.size() + 1;
    pending_.clear();
    pending_bytes_ = 0;
    update_active_ = false;
    return ReceiveFlowRoute::kClose;
  }
  if (packet.packet.size() > kMaximumBytes) {
    ++dropped_count_;
    return ReceiveFlowRoute::kBuffered;
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
  flow_ids_ = std::move(flow_ids);
  update_active_ = false;
  std::vector<RoqReceivedPacket> released;
  for (BufferedPacket& buffered : pending_) {
    if (IsKnown(buffered.packet.flow_id)) {
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
  return std::find(flow_ids_.begin(), flow_ids_.end(), flow_id) !=
         flow_ids_.end();
}

void ReceiveFlowRegistry::DropOldest() {
  pending_bytes_ -= pending_.front().bytes;
  pending_.pop_front();
  ++dropped_count_;
}

}  // namespace webrtc::roq
