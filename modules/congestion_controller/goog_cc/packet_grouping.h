/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_PACKET_GROUPING_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_PACKET_GROUPING_H_

#include <deque>
#include <vector>

#include "api/transport/network_control.h"

namespace webrtc {
struct PacketDelayGroup {
  explicit PacketDelayGroup(PacketResult packet, Timestamp feedback_time);

  PacketDelayGroup(const PacketDelayGroup&);
  ~PacketDelayGroup();
  void AddPacketInfo(PacketResult packet, Timestamp feedback_time);
  bool BelongsToGroup(PacketResult packet) const;
  bool BelongsToBurst(PacketResult packet) const;

  Timestamp first_send_time;
  Timestamp last_send_time;

  Timestamp first_receive_time;
  Timestamp last_receive_time;
  Timestamp last_feedback_time;
};

struct PacketDelayDelta {
  Timestamp receive_time = Timestamp::PlusInfinity();
  TimeDelta send = TimeDelta::Zero();
  TimeDelta receive = TimeDelta::Zero();
  TimeDelta feedback = TimeDelta::Zero();
};

class PacketDelayGrouper {
 public:
  PacketDelayGrouper();
  ~PacketDelayGrouper();
  void AddPacketInfo(PacketResult packet, Timestamp feedback_time);
  std::vector<PacketDelayDelta> PopDeltas();
  void Reset() { packet_groups_.clear(); }

 private:
  std::deque<PacketDelayGroup> packet_groups_;
  int num_consecutive_reordered_packets_ = 0;
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_GOOG_CC_PACKET_GROUPING_H_
