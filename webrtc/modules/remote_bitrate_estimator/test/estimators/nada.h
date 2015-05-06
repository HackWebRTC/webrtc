/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
*/

//  Implementation of Network-Assisted Dynamic Adaptation's (NADA's) proposal
//  Version according to Draft Document (mentioned in references)
//  http://tools.ietf.org/html/draft-zhu-rmcat-nada-06
//  From March 26, 2015.

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_NADA_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_NADA_H_

#include <list>
#include <map>

#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe.h"
#include "webrtc/voice_engine/channel.h"

namespace webrtc {

class ReceiveStatistics;

namespace testing {
namespace bwe {

// Holds only essential information about packets to be saved for
// further use, e.g. for calculating packet loss and receiving rate.
struct PacketIdentifierNode {
  PacketIdentifierNode(uint16_t sequence_number,
                       int64_t send_time_ms,
                       int64_t arrival_time_ms,
                       size_t payload_size)
      : sequence_number_(sequence_number),
        send_time_ms_(send_time_ms),
        arrival_time_ms_(arrival_time_ms),
        payload_size_(payload_size) {}

  uint16_t sequence_number_;
  int64_t send_time_ms_;
  int64_t arrival_time_ms_;
  size_t payload_size_;
};

typedef std::list<PacketIdentifierNode*>::iterator PacketNodeIt;

// FIFO implementation for a limited capacity set.
// Used for keeping the latest arrived packets while avoiding duplicates.
// Allows efficient insertion, deletion and search.
class LinkedSet {
 public:
  explicit LinkedSet(size_t capacity) : capacity_(capacity) {}

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

 private:
  // Pop oldest element from the back of the list and remove it from the map.
  void RemoveTail();
  // Add new element to the front of the list and insert it in the map.
  void UpdateHead(PacketIdentifierNode* new_head);
  size_t capacity_;
  std::map<uint16_t, PacketNodeIt> map_;
  std::list<PacketIdentifierNode*> list_;
};

class NadaBweReceiver : public BweReceiver {
 public:
  explicit NadaBweReceiver(int flow_id);
  virtual ~NadaBweReceiver();

  void ReceivePacket(int64_t arrival_time_ms,
                     const MediaPacket& media_packet) override;
  FeedbackPacket* GetFeedback(int64_t now_ms) override;
  float GlobalPacketLossRatio();
  float RecentPacketLossRatio();
  size_t RecentReceivingRate();
  static int64_t MedianFilter(int64_t* v, int size);
  static int64_t ExponentialSmoothingFilter(int64_t new_value,
                                            int64_t last_smoothed_value,
                                            float alpha);

 private:
  SimulatedClock clock_;
  int64_t last_feedback_ms_;
  rtc::scoped_ptr<ReceiveStatistics> recv_stats_;
  int64_t baseline_delay_ms_;  // Referred as d_f.
  int64_t delay_signal_ms_;    // Referred as d_n.
  int64_t last_congestion_signal_ms_;
  int last_delays_index_;
  int64_t exp_smoothed_delay_ms_;        // Referred as d_hat_n.
  int64_t est_queuing_delay_signal_ms_;  // Referred as d_tilde_n.
  // Deals with packets sent more than once.
  static const int kSetCapacity = 10000;  // Lower than 0xFFFF / 2.
  LinkedSet* received_packets_ = new LinkedSet(kSetCapacity);
  static const int kMedian = 5;      // Used for k-points Median Filter.
  int64_t last_delays_ms_[kMedian];  // Used for Median Filter.
};

class NadaBweSender : public BweSender {
 public:
  NadaBweSender(int kbps, BitrateObserver* observer, Clock* clock);
  NadaBweSender(BitrateObserver* observer, Clock* clock);
  virtual ~NadaBweSender();

  int GetFeedbackIntervalMs() const override;
  // Updates the min_feedback_delay_ms_ and the min_round_trip_time_ms_.
  void GiveFeedback(const FeedbackPacket& feedback) override;
  void OnPacketsSent(const Packets& packets) override {}
  int64_t TimeUntilNextProcess() override;
  int Process() override;
  void AcceleratedRampUp(const NadaFeedback& fb, const int kMaxRefRateKbps);
  void GradualRateUpdate(const NadaFeedback& fb,
                         const int kMaxRefRateKbps,
                         const float delta_s);

 private:
  Clock* const clock_;
  BitrateObserver* const observer_;
  // Referred as R_min, default initialization for bitrate R_n.
  const int kMinRefRateKbps = 150;
  int bitrate_kbps_;  // Referred as "Reference Rate" = R_n.
  int64_t last_feedback_ms_ = 0;
  // Referred as delta_0, initialized as an upper bound.
  int64_t min_feedback_delay_ms_ = 5000;
  // Referred as RTT_0, initialized as an upper bound.
  int64_t min_round_trip_time_ms_ = 500;

  DISALLOW_IMPLICIT_CONSTRUCTORS(NadaBweSender);
};

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_NADA_H_
