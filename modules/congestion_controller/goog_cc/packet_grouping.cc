/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/goog_cc/packet_grouping.h"

#include <algorithm>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {
constexpr TimeDelta kMaxSendTimeGroupDuration = TimeDelta::Millis<5>();
constexpr TimeDelta kMaxReceiveTimeBurstDelta = TimeDelta::Millis<5>();
constexpr TimeDelta kMaxReceiveTimeBurstDuration = TimeDelta::Millis<100>();
constexpr TimeDelta kReceiveTimeOffsetThreshold = TimeDelta::Millis<3000>();
constexpr int kReorderedResetThreshold = 3;
}  // namespace

PacketDelayGroup::PacketDelayGroup(PacketResult packet, Timestamp feedback_time)
    : first_send_time(packet.sent_packet.send_time),
      last_send_time(packet.sent_packet.send_time),
      first_receive_time(packet.receive_time),
      last_receive_time(packet.receive_time),
      last_feedback_time(feedback_time) {}

PacketDelayGroup::~PacketDelayGroup() = default;
PacketDelayGroup::PacketDelayGroup(const PacketDelayGroup&) = default;

void PacketDelayGroup::AddPacketInfo(PacketResult packet,
                                     Timestamp feedback_time) {
  last_send_time = std::max(last_send_time, packet.sent_packet.send_time);
  first_receive_time = std::min(first_receive_time, packet.receive_time);
  last_receive_time = std::max(last_receive_time, packet.receive_time);
  last_feedback_time = std::max(last_feedback_time, feedback_time);
}

bool PacketDelayGroup::BelongsToGroup(PacketResult packet) const {
  TimeDelta send_time_duration = packet.sent_packet.send_time - first_send_time;
  return send_time_duration <= kMaxSendTimeGroupDuration;
}

bool PacketDelayGroup::BelongsToBurst(PacketResult packet) const {
  TimeDelta send_time_delta = packet.sent_packet.send_time - first_send_time;
  TimeDelta receive_time_delta = packet.receive_time - last_receive_time;
  TimeDelta receive_time_duration = packet.receive_time - first_receive_time;
  bool receiving_faster_than_sent = receive_time_delta < send_time_delta;
  return receiving_faster_than_sent &&
         receive_time_delta <= kMaxReceiveTimeBurstDelta &&
         receive_time_duration <= kMaxReceiveTimeBurstDuration;
}

PacketDelayGrouper::PacketDelayGrouper() = default;

PacketDelayGrouper::~PacketDelayGrouper() = default;

void PacketDelayGrouper::AddPacketInfo(PacketResult packet,
                                       Timestamp feedback_time) {
  if (packet_groups_.empty()) {
    packet_groups_.emplace_back(packet, feedback_time);
  } else if (packet.sent_packet.send_time >=
             packet_groups_.back().first_send_time) {
    if (packet_groups_.back().BelongsToGroup(packet) ||
        packet_groups_.back().BelongsToBurst(packet)) {
      packet_groups_.back().AddPacketInfo(packet, feedback_time);
    } else {
      packet_groups_.emplace_back(packet, feedback_time);
    }
  }
}

std::vector<PacketDelayDelta> PacketDelayGrouper::PopDeltas() {
  std::vector<PacketDelayDelta> deltas;
  while (packet_groups_.size() >= 3) {
    PacketDelayDelta delta;
    delta.receive_time = packet_groups_[1].last_receive_time;
    delta.send =
        packet_groups_[1].last_send_time - packet_groups_[0].last_send_time;
    delta.receive = packet_groups_[1].last_receive_time -
                    packet_groups_[0].last_receive_time;
    delta.feedback = packet_groups_[1].last_feedback_time -
                     packet_groups_[0].last_feedback_time;
    packet_groups_.pop_front();

    if (delta.receive - delta.feedback >= kReceiveTimeOffsetThreshold) {
      RTC_LOG(LS_WARNING) << "The receive clock offset has changed (diff = "
                          << ToString(delta.receive - delta.feedback)
                          << "), resetting.";
      packet_groups_.pop_front();
    } else if (delta.receive < TimeDelta::Zero()) {
      ++num_consecutive_reordered_packets_;
      if (num_consecutive_reordered_packets_ >= kReorderedResetThreshold) {
        RTC_LOG(LS_WARNING) << "Decreasing receive time in multiple "
                               "consecutive packet groups, resetting";
        packet_groups_.pop_front();
      }
    } else {
      num_consecutive_reordered_packets_ = 0;
      deltas.push_back(delta);
    }
  }
  return deltas;
}

}  // namespace webrtc
