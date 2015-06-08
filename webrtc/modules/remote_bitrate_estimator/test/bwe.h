/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_H_

#include <sstream>

#include "webrtc/modules/remote_bitrate_estimator/test/packet.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"

namespace webrtc {
namespace testing {
namespace bwe {

// Holds only essential information about packets to be saved for
// further use, e.g. for calculating packet loss and receiving rate.
struct PacketIdentifierNode {
  PacketIdentifierNode(uint16_t sequence_number,
                       int64_t send_time_ms,
                       int64_t arrival_time_ms,
                       size_t payload_size)
      : sequence_number(sequence_number),
        send_time_ms(send_time_ms),
        arrival_time_ms(arrival_time_ms),
        payload_size(payload_size) {}

  uint16_t sequence_number;
  int64_t send_time_ms;
  int64_t arrival_time_ms;
  size_t payload_size;
};

typedef std::list<PacketIdentifierNode*>::iterator PacketNodeIt;

// FIFO implementation for a limited capacity set.
// Used for keeping the latest arrived packets while avoiding duplicates.
// Allows efficient insertion, deletion and search.
class LinkedSet {
 public:
  explicit LinkedSet(int capacity) : capacity_(capacity) {}

  // If the arriving packet (identified by its sequence number) is already
  // in the LinkedSet, move its Node to the head of the list. Else, create
  // a PacketIdentifierNode n_ and then UpdateHead(n_), calling RemoveTail()
  // if the LinkedSet reached its maximum capacity.
  void Insert(uint16_t sequence_number,
              int64_t send_time_ms,
              int64_t arrival_time_ms,
              size_t payload_size);

  PacketNodeIt begin() { return list_.begin(); }
  PacketNodeIt end() { return list_.end(); }
  bool empty() { return list_.empty(); }
  size_t size() { return list_.size(); }
  // Gets the latest arrived sequence number.
  uint16_t find_max() { return map_.rbegin()->first; }
  // Gets the first arrived sequence number still saved in the LinkedSet.
  uint16_t find_min() { return map_.begin()->first; }
  // Gets the lowest saved sequence number that is >= than the input key.
  uint16_t lower_bound(uint16_t key) { return map_.lower_bound(key)->first; }
  // Gets the highest saved sequence number that is <= than the input key.
  uint16_t upper_bound(uint16_t key) { return map_.upper_bound(key)->first; }
  size_t capacity() { return capacity_; }

 private:
  // Pop oldest element from the back of the list and remove it from the map.
  void RemoveTail();
  // Add new element to the front of the list and insert it in the map.
  void UpdateHead(PacketIdentifierNode* new_head);
  size_t capacity_;
  std::map<uint16_t, PacketNodeIt> map_;
  std::list<PacketIdentifierNode*> list_;
};

const int kMinBitrateKbps = 150;
const int kMaxBitrateKbps = 3000;

class BweSender : public Module {
 public:
  BweSender() {}
  virtual ~BweSender() {}

  virtual int GetFeedbackIntervalMs() const = 0;
  virtual void GiveFeedback(const FeedbackPacket& feedback) = 0;
  virtual void OnPacketsSent(const Packets& packets) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BweSender);
};

class BweReceiver {
 public:
  explicit BweReceiver(int flow_id);
  virtual ~BweReceiver() {}

  virtual void ReceivePacket(int64_t arrival_time_ms,
                             const MediaPacket& media_packet) {}
  virtual FeedbackPacket* GetFeedback(int64_t now_ms) { return NULL; }

  float GlobalPacketLossRatio();
  float RecentPacketLossRatio();
  size_t GetSetCapacity() { return received_packets_.capacity(); }

  static const int64_t kPacketLossTimeWindowMs = 500;

 protected:
  int flow_id_;
  // Deals with packets sent more than once.
  LinkedSet received_packets_;
};

enum BandwidthEstimatorType {
  kNullEstimator,
  kNadaEstimator,
  kRembEstimator,
  kFullSendSideEstimator,
  kTcpEstimator
};

int64_t GetAbsSendTimeInMs(uint32_t abs_send_time);

BweSender* CreateBweSender(BandwidthEstimatorType estimator,
                           int kbps,
                           BitrateObserver* observer,
                           Clock* clock);

BweReceiver* CreateBweReceiver(BandwidthEstimatorType type,
                               int flow_id,
                               bool plot);
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_H_
