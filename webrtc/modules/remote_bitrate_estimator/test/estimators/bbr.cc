/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "webrtc/modules/remote_bitrate_estimator/test/estimators/bbr.h"

#include "webrtc/modules/remote_bitrate_estimator/test/estimators/max_bandwidth_filter.h"

namespace webrtc {
namespace testing {
namespace bwe {
namespace {
const int kFeedbackIntervalsMs = 100;
// BBR uses this value to double sending rate each round trip. Design document
// suggests using this value.
const float kHighGain = 2.885f;
// BBR uses this value to drain queues created during STARTUP in one round trip
// time.
const float kDrainGain = 1 / kHighGain;
// kStartupGrowthTarget and kMaxRoundsWithoutGrowth are chosen from
// experiments, according to the design document.
const float kStartupGrowthTarget = 1.25f;
const int kMaxRoundsWithoutGrowth = 3;
}  // namespace

BbrBweSender::BbrBweSender(Clock* clock)
    : BweSender(0),
      clock_(clock),
      mode_(STARTUP),
      max_bandwidth_filter_(new MaxBandwidthFilter()),
      round_count_(0),
      last_packet_sent_(0),
      round_trip_end_(0),
      full_bandwidth_reached_(false) {
  // Initially enter Startup mode.
  EnterStartup();
}

BbrBweSender::~BbrBweSender() {}

int BbrBweSender::GetFeedbackIntervalMs() const {
  return kFeedbackIntervalsMs;
}

void BbrBweSender::GiveFeedback(const FeedbackPacket& feedback) {
  const BbrBweFeedback& fb = static_cast<const BbrBweFeedback&>(feedback);
  // feedback_vector holds values of acknowledged packets' sequence numbers.
  const std::vector<uint64_t>& feedback_vector = fb.packet_feedback_vector();
  // Check if new round started for the connection. Round is the period of time
  // from sending packet to its acknowledgement.
  bool new_round_started = false;
  if (!feedback_vector.empty()) {
    uint64_t last_acked_packet = *feedback_vector.rbegin();
    if (last_acked_packet > round_trip_end_) {
      new_round_started = true;
      round_count_++;
      round_trip_end_ = last_packet_sent_;
    }
  }
  if (new_round_started && !full_bandwidth_reached_) {
    full_bandwidth_reached_ = max_bandwidth_filter_->FullBandwidthReached(
        kStartupGrowthTarget, kMaxRoundsWithoutGrowth);
  }
  int now_ms = clock_->TimeInMilliseconds();
  switch (mode_) {
    break;
    case STARTUP:
      TryExitingStartup();
      break;
    case DRAIN:
      TryExitingDrain(now_ms);
      break;
    case PROBE_BW:
      TryUpdatingCyclePhase(now_ms);
      break;
    case PROBE_RTT:
      TryExitingProbeRtt(now_ms);
      break;
  }
  TryEnteringProbeRtt(now_ms);
  // TODO(gnish): implement functions updating congestion window and pacing rate
  // controllers.
}

bool BbrBweSender::UpdateBandwidthAndMinRtt() {
  return false;
}

void BbrBweSender::EnterStartup() {
  mode_ = STARTUP;
  pacing_gain_ = kHighGain;
  congestion_window_gain_ = kHighGain;
}

void BbrBweSender::TryExitingStartup() {
  if (full_bandwidth_reached_) {
    mode_ = DRAIN;
    pacing_gain_ = kDrainGain;
    congestion_window_gain_ = kHighGain;
  }
}

void BbrBweSender::TryExitingDrain(int64_t now_ms) {}

void BbrBweSender::EnterProbeBw(int64_t now_ms) {}

void BbrBweSender::TryUpdatingCyclePhase(int64_t now_ms) {}

void BbrBweSender::TryEnteringProbeRtt(int64_t now_ms) {}
void BbrBweSender::TryExitingProbeRtt(int64_t now_ms) {}

int64_t BbrBweSender::TimeUntilNextProcess() {
  return 100;
}

void BbrBweSender::OnPacketsSent(const Packets& packets) {
  last_packet_sent_ =
      static_cast<const MediaPacket*>(packets.back())->sequence_number();
}

void BbrBweSender::Process() {}

BbrBweReceiver::BbrBweReceiver(int flow_id)
    : BweReceiver(flow_id, kReceivingRateTimeWindowMs), clock_(0) {}

BbrBweReceiver::~BbrBweReceiver() {}

void BbrBweReceiver::ReceivePacket(int64_t arrival_time_ms,
                                   const MediaPacket& media_packet) {}

FeedbackPacket* BbrBweReceiver::GetFeedback(int64_t now_ms) {
  return nullptr;
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
