/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef MODULES_ROQ_RECEIVE_FLOW_REGISTRY_H_
#define MODULES_ROQ_RECEIVE_FLOW_REGISTRY_H_

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <set>
#include <vector>

#include "api/roq/roq_session.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc::roq {

enum class ReceiveFlowPolicy { kDeliver, kDiscard };
enum class ReceiveFlowRoute { kDeliver, kBuffered, kDiscard, kClose };

struct ReceiveFlowConfig {
  uint64_t flow_id = 0;
  ReceiveFlowPolicy policy = ReceiveFlowPolicy::kDeliver;
};

class ReceiveFlowRegistry {
 public:
  explicit ReceiveFlowRegistry(std::vector<uint64_t> flow_ids);
  ReceiveFlowRegistry(std::vector<uint64_t> flow_ids, bool buffer_dynamic_flows);

  void BeginUpdate(Timestamp now);
  void Prepare(std::vector<uint64_t> flow_ids);
  ReceiveFlowRoute Route(const RoqReceivedPacket& packet, Timestamp now);
  std::vector<RoqReceivedPacket> Update(std::vector<uint64_t> flow_ids);
  std::vector<RoqReceivedPacket> Update(std::vector<uint64_t> flow_ids, Timestamp now);
  std::vector<RoqReceivedPacket> Update(
      std::vector<ReceiveFlowConfig> flow_configs, Timestamp now);
  std::vector<RoqReceivedPacket> Update(
      std::vector<ReceiveFlowConfig> flow_configs);

  size_t buffered() const { return buffered_count_; }
  size_t released() const { return released_count_; }
  size_t dropped() const { return dropped_count_; }

 private:
  struct BufferedPacket {
    RoqReceivedPacket packet;
    size_t bytes = 0;
    Timestamp received_at = Timestamp::MinusInfinity();
  };

  bool IsKnown(uint64_t flow_id) const;
  void DropOldest();
  void DropOldestForFlow(uint64_t flow_id);
  ReceiveFlowRoute RecordUnknown(const RoqReceivedPacket& packet,
                                 Timestamp now);
  void ExpirePending(Timestamp now);

  static constexpr size_t kMaximumPackets = 32;
  static constexpr size_t kMaximumPacketsPerFlow = 24;
  static constexpr size_t kMaximumBytes = 64 * 1024;
  static constexpr TimeDelta kGraceDuration = TimeDelta::Millis(100);
  static constexpr size_t kUnknownMaximumPackets = 32;
  static constexpr size_t kUnknownMaximumBytes = 64 * 1024;
  static constexpr TimeDelta kUnknownWindow = TimeDelta::Seconds(1);

  std::map<uint64_t, ReceiveFlowPolicy> flows_;
  std::set<uint64_t> retired_flows_;
  std::set<uint64_t> preparing_flows_;
  bool buffer_dynamic_flows_ = false;
  bool unknown_abuse_ = false;
  std::deque<BufferedPacket> pending_;
  size_t pending_bytes_ = 0;
  size_t buffered_count_ = 0;
  size_t released_count_ = 0;
  size_t dropped_count_ = 0;
  bool update_active_ = false;
  Timestamp update_deadline_ = Timestamp::MinusInfinity();
  Timestamp unknown_window_start_ = Timestamp::MinusInfinity();
  size_t unknown_packets_ = 0;
  size_t unknown_bytes_ = 0;
};

}  // namespace webrtc::roq

#endif  // MODULES_ROQ_RECEIVE_FLOW_REGISTRY_H_
