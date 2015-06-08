/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/bwe.h"

#include <limits>

#include "webrtc/base/common.h"
#include "webrtc/modules/remote_bitrate_estimator/test/estimators/nada.h"
#include "webrtc/modules/remote_bitrate_estimator/test/estimators/remb.h"
#include "webrtc/modules/remote_bitrate_estimator/test/estimators/send_side.h"
#include "webrtc/modules/remote_bitrate_estimator/test/estimators/tcp.h"

namespace webrtc {
namespace testing {
namespace bwe {

// With the assumption that packet loss is lower than 97%, the max gap
// between elements in the set is lower than 0x8000, hence we have a
// total order in the set. For (x,y,z) subset of the LinkedSet,
// (x<=y and y<=z) ==> x<=z so the set can be sorted.
const int kSetCapacity = 1000;

BweReceiver::BweReceiver(int flow_id)
    : flow_id_(flow_id), received_packets_(kSetCapacity) {
}

class NullBweSender : public BweSender {
 public:
  NullBweSender() {}
  virtual ~NullBweSender() {}

  int GetFeedbackIntervalMs() const override { return 1000; }
  void GiveFeedback(const FeedbackPacket& feedback) override {}
  void OnPacketsSent(const Packets& packets) override {}
  int64_t TimeUntilNextProcess() override {
    return std::numeric_limits<int64_t>::max();
  }
  int Process() override { return 0; }

 private:
  DISALLOW_COPY_AND_ASSIGN(NullBweSender);
};

int64_t GetAbsSendTimeInMs(uint32_t abs_send_time) {
  const int kInterArrivalShift = 26;
  const int kAbsSendTimeInterArrivalUpshift = 8;
  const double kTimestampToMs =
      1000.0 / static_cast<double>(1 << kInterArrivalShift);
  uint32_t timestamp = abs_send_time << kAbsSendTimeInterArrivalUpshift;
  return static_cast<int64_t>(timestamp) * kTimestampToMs;
}

BweSender* CreateBweSender(BandwidthEstimatorType estimator,
                           int kbps,
                           BitrateObserver* observer,
                           Clock* clock) {
  switch (estimator) {
    case kRembEstimator:
      return new RembBweSender(kbps, observer, clock);
    case kFullSendSideEstimator:
      return new FullBweSender(kbps, observer, clock);
    case kNadaEstimator:
      return new NadaBweSender(kbps, observer, clock);
    case kTcpEstimator:
      FALLTHROUGH();
    case kNullEstimator:
      return new NullBweSender();
  }
  assert(false);
  return NULL;
}

BweReceiver* CreateBweReceiver(BandwidthEstimatorType type,
                               int flow_id,
                               bool plot) {
  switch (type) {
    case kRembEstimator:
      return new RembReceiver(flow_id, plot);
    case kFullSendSideEstimator:
      return new SendSideBweReceiver(flow_id);
    case kNadaEstimator:
      return new NadaBweReceiver(flow_id);
    case kTcpEstimator:
      return new TcpBweReceiver(flow_id);
    case kNullEstimator:
      return new BweReceiver(flow_id);
  }
  assert(false);
  return NULL;
}

float BweReceiver::GlobalPacketLossRatio() {
  if (received_packets_.empty()) {
    return 0.0f;
  }
  // Possibly there are packets missing.
  const uint16_t kMaxGap = 1.5 * kSetCapacity;
  uint16_t min = received_packets_.find_min();
  uint16_t max = received_packets_.find_max();

  int gap;
  if (max - min < kMaxGap) {
    gap = max - min + 1;
  } else {  // There was an overflow.
    max = received_packets_.upper_bound(kMaxGap);
    min = received_packets_.lower_bound(0xFFFF - kMaxGap);
    gap = max + (0xFFFF - min) + 2;
  }
  return static_cast<float>(received_packets_.size()) / gap;
}

// Go through a fixed time window of most recent packets received and
// counts packets missing to obtain the packet loss ratio. If an unordered
// packet falls out of the timewindow it will be counted as missing.
// E.g.: for a timewindow covering 5 packets of the following arrival sequence
// {10 7 9 5 6} 8 3 2 4 1, the output will be 1/6 (#8 is considered as missing).
float BweReceiver::RecentPacketLossRatio() {
  if (received_packets_.empty()) {
    return 0.0f;
  }
  int number_packets_received = 0;

  PacketNodeIt node_it = received_packets_.begin();  // Latest.

  // Lowest timestamp limit, oldest one that should be checked.
  int64_t time_limit_ms = (*node_it)->arrival_time_ms - kPacketLossTimeWindowMs;
  // Oldest and newest values found within the given time window.
  uint16_t oldest_seq_nb = (*node_it)->sequence_number;
  uint16_t newest_seq_nb = oldest_seq_nb;

  while (node_it != received_packets_.end()) {
    if ((*node_it)->arrival_time_ms < time_limit_ms) {
      break;
    }
    uint16_t seq_nb = (*node_it)->sequence_number;
    if (IsNewerSequenceNumber(seq_nb, newest_seq_nb)) {
      newest_seq_nb = seq_nb;
    }
    if (IsNewerSequenceNumber(oldest_seq_nb, seq_nb)) {
      oldest_seq_nb = seq_nb;
    }
    ++node_it;
    ++number_packets_received;
  }
  // Interval width between oldest and newest sequence number.
  // There was an overflow if newest_seq_nb < oldest_seq_nb.
  int gap = static_cast<uint16_t>(newest_seq_nb - oldest_seq_nb + 1);

  return static_cast<float>(gap - number_packets_received) / gap;
}

void LinkedSet::Insert(uint16_t sequence_number,
                       int64_t send_time_ms,
                       int64_t arrival_time_ms,
                       size_t payload_size) {
  std::map<uint16_t, PacketNodeIt>::iterator it = map_.find(sequence_number);
  if (it != map_.end()) {
    PacketNodeIt node_it = it->second;
    PacketIdentifierNode* node = *node_it;
    node->arrival_time_ms = arrival_time_ms;
    if (node_it != list_.begin()) {
      list_.erase(node_it);
      list_.push_front(node);
      map_[sequence_number] = list_.begin();
    }
  } else {
    if (size() == capacity_) {
      RemoveTail();
    }
    UpdateHead(new PacketIdentifierNode(sequence_number, send_time_ms,
                                        arrival_time_ms, payload_size));
  }
}
void LinkedSet::RemoveTail() {
  map_.erase(list_.back()->sequence_number);
  list_.pop_back();
}
void LinkedSet::UpdateHead(PacketIdentifierNode* new_head) {
  list_.push_front(new_head);
  map_[new_head->sequence_number] = list_.begin();
}

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
