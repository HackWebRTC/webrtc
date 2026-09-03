/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef MODULES_ROQ_RECEIVE_FLOW_REGISTRY_H_
#define MODULES_ROQ_RECEIVE_FLOW_REGISTRY_H_

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

#include "api/roq/roq_session.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc::roq {

enum class ReceiveFlowRoute { kDeliver, kBuffered, kClose };

class ReceiveFlowRegistry {
 public:
  explicit ReceiveFlowRegistry(std::vector<uint64_t> flow_ids);

  void BeginUpdate(Timestamp now);
  ReceiveFlowRoute Route(const RoqReceivedPacket& packet, Timestamp now);
  std::vector<RoqReceivedPacket> Update(std::vector<uint64_t> flow_ids);

  size_t buffered() const { return buffered_count_; }
  size_t released() const { return released_count_; }
  size_t dropped() const { return dropped_count_; }

 private:
  struct BufferedPacket {
    RoqReceivedPacket packet;
    size_t bytes = 0;
  };

  bool IsKnown(uint64_t flow_id) const;
  void DropOldest();

  static constexpr size_t kMaximumPackets = 32;
  static constexpr size_t kMaximumBytes = 64 * 1024;
  static constexpr TimeDelta kGraceDuration = TimeDelta::Millis(100);

  std::vector<uint64_t> flow_ids_;
  std::deque<BufferedPacket> pending_;
  size_t pending_bytes_ = 0;
  size_t buffered_count_ = 0;
  size_t released_count_ = 0;
  size_t dropped_count_ = 0;
  bool update_active_ = false;
  Timestamp update_deadline_ = Timestamp::MinusInfinity();
};

}  // namespace webrtc::roq

#endif  // MODULES_ROQ_RECEIVE_FLOW_REGISTRY_H_
