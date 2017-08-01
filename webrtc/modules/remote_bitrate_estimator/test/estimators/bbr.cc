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

#include <stdlib.h>

#include "webrtc/modules/remote_bitrate_estimator/test/estimators/congestion_window.h"
#include "webrtc/modules/remote_bitrate_estimator/test/estimators/max_bandwidth_filter.h"
#include "webrtc/modules/remote_bitrate_estimator/test/estimators/min_rtt_filter.h"

namespace webrtc {
namespace testing {
namespace bwe {
namespace {
const int kFeedbackIntervalsMs = 3;
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
// Pacing gain values for Probe Bandwidth mode.
const float kPacingGain[] = {1.25, 0.75, 1, 1, 1, 1, 1, 1};
const size_t kGainCycleLength = sizeof(kPacingGain) / sizeof(kPacingGain[0]);
// The least amount of rounds PROBE_RTT mode should last.
const int kProbeRttDurationRounds = 1;
// The least amount of milliseconds PROBE_RTT mode should last.
const int kProbeRttDurationMs = 200;
// Gain value for congestion window for assuming that network has no queues.
const float kTargetCongestionWindowGain = 1;
// Gain value for congestion window in PROBE_BW mode. In theory it should be
// equal to 1, but in practice because of delayed acks and the way networks
// work, it is nice to have some extra room in congestion window for full link
// utilization. Value chosen by observations on different tests.
const float kCruisingCongestionWindowGain = 1.5f;
// Expiration time for min_rtt sample, which is set to 10 seconds according to
// BBR design doc.
const int64_t kMinRttFilterSizeMs = 10000;
}  // namespace

BbrBweSender::BbrBweSender(Clock* clock)
    : BweSender(0),
      clock_(clock),
      mode_(STARTUP),
      max_bandwidth_filter_(new MaxBandwidthFilter()),
      min_rtt_filter_(new MinRttFilter()),
      congestion_window_(new CongestionWindow()),
      rand_(new Random(time(NULL))),
      round_count_(0),
      last_packet_sent_(0),
      round_trip_end_(0),
      full_bandwidth_reached_(false),
      cycle_start_time_ms_(0),
      cycle_index_(0),
      prior_in_flight_(0),
      probe_rtt_start_time_ms_(0),
      minimum_congestion_window_start_time_ms_(),
      minimum_congestion_window_start_round_(0) {
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
      TryExitingProbeRtt(now_ms, 0);
      break;
  }
  TryEnteringProbeRtt(now_ms);
  // TODO(gnish): implement functions updating congestion window and pacing rate
  // controllers.
}

size_t BbrBweSender::TargetCongestionWindow(float gain) {
  size_t target_congestion_window =
      congestion_window_->GetTargetCongestionWindow(
          max_bandwidth_filter_->max_bandwidth_estimate_bps(),
          min_rtt_filter_->min_rtt_ms(), gain);
  return target_congestion_window;
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

void BbrBweSender::TryExitingDrain(int64_t now_ms) {
  if (congestion_window_->data_inflight() <=
      TargetCongestionWindow(kTargetCongestionWindowGain))
    EnterProbeBw(now_ms);
}

// Start probing with a random gain value, which is different form 0.75,
// starting with 0.75 doesn't offer any benefits as there are no queues to be
// drained.
void BbrBweSender::EnterProbeBw(int64_t now_ms) {
  mode_ = PROBE_BW;
  congestion_window_gain_ = kCruisingCongestionWindowGain;
  int index = rand_->Rand(kGainCycleLength - 2);
  if (index == 1)
    index = kGainCycleLength - 1;
  pacing_gain_ = kPacingGain[index];
  cycle_start_time_ms_ = now_ms;
  cycle_index_ = index;
}

void BbrBweSender::TryUpdatingCyclePhase(int64_t now_ms) {
  // Each phase should last rougly min_rtt ms time.
  bool advance_cycle_phase = false;
  if (min_rtt_filter_->min_rtt_ms())
    advance_cycle_phase =
        now_ms - cycle_start_time_ms_ > *min_rtt_filter_->min_rtt_ms();
  // If BBR was probing and it couldn't increase data inflight sufficiently in
  // one min_rtt time, continue probing. BBR design doc isn't clear about this,
  // but condition helps in quicker ramp-up and performs better.
  if (pacing_gain_ > 1.0 &&
      prior_in_flight_ < TargetCongestionWindow(pacing_gain_))
    advance_cycle_phase = false;
  // If BBR has already drained queues there is no point in continuing draining
  // phase.
  if (pacing_gain_ < 1.0 && prior_in_flight_ <= TargetCongestionWindow(1))
    advance_cycle_phase = true;
  if (advance_cycle_phase) {
    cycle_index_++;
    cycle_index_ %= kGainCycleLength;
    pacing_gain_ = kPacingGain[cycle_index_];
    cycle_start_time_ms_ = now_ms;
  }
}

void BbrBweSender::TryEnteringProbeRtt(int64_t now_ms) {
  if (min_rtt_filter_->min_rtt_expired(now_ms, kMinRttFilterSizeMs) &&
      mode_ != PROBE_RTT) {
    mode_ = PROBE_RTT;
    pacing_gain_ = 1;
    probe_rtt_start_time_ms_ = now_ms;
    minimum_congestion_window_start_time_ms_.reset();
  }
}

// minimum_congestion_window_start_time_'s value is set to the first moment when
// data inflight was less then kMinimumCongestionWindowBytes, we should make
// sure that BBR has been in PROBE_RTT mode for at least one round or 200ms.
void BbrBweSender::TryExitingProbeRtt(int64_t now_ms, int64_t round) {
  if (!minimum_congestion_window_start_time_ms_) {
    if (congestion_window_->data_inflight() <=
        CongestionWindow::kMinimumCongestionWindowBytes) {
      *minimum_congestion_window_start_time_ms_ = now_ms;
      minimum_congestion_window_start_round_ = round;
    }
  } else {
    if (now_ms - *minimum_congestion_window_start_time_ms_ >=
            kProbeRttDurationMs &&
        round - minimum_congestion_window_start_round_ >=
            kProbeRttDurationRounds)
      EnterProbeBw(now_ms);
  }
}

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
