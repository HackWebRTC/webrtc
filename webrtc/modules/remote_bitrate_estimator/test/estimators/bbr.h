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

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_BBR_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_BBR_H_

#include <map>
#include <memory>
#include <vector>

#include "webrtc/modules/remote_bitrate_estimator/test/bwe.h"

namespace webrtc {
namespace testing {
namespace bwe {
class MaxBandwidthFilter;
class MinRttFilter;
class CongestionWindow;
class BbrBweSender : public BweSender {
 public:
  explicit BbrBweSender(Clock* clock);
  virtual ~BbrBweSender();
  enum Mode {
    // Startup phase.
    STARTUP,
    // Queue draining phase, which where created during startup.
    DRAIN,
    // Cruising, probing new bandwidth.
    PROBE_BW,
    // Temporarily limiting congestion window size in order to measure
    // minimum RTT.
    PROBE_RTT
  };
  struct PacketStats {
    PacketStats() {}
    PacketStats(int64_t send_time_, size_t payload_size_)
        : send_time(send_time_), payload_size(payload_size_) {}

    int64_t send_time;
    size_t payload_size;
  };
  void OnPacketsSent(const Packets& packets) override;
  int GetFeedbackIntervalMs() const override;
  void GiveFeedback(const FeedbackPacket& feedback) override;
  int64_t TimeUntilNextProcess() override;
  void Process() override;

 private:
  void EnterStartup();
  bool UpdateBandwidthAndMinRtt();
  void TryExitingStartup();
  void TryExitingDrain(int64_t now_ms);
  void EnterProbeBw(int64_t now_ms);
  void EnterProbeRtt(int64_t now_ms);
  void TryUpdatingCyclePhase(int64_t now_ms);
  void TryEnteringProbeRtt(int64_t now_ms);
  void TryExitingProbeRtt(int64_t now_ms);
  Clock* const clock_;
  Mode mode_;
  std::unique_ptr<MaxBandwidthFilter> max_bandwidth_filter_;
  uint64_t round_count_;
  uint64_t last_packet_sent_;
  uint64_t round_trip_end_;
  float pacing_gain_;
  float congestion_window_gain_;

  // If optimal bandwidth has been discovered and reached, (for example after
  // Startup mode) set this variable to true.
  bool full_bandwidth_reached_;
};

class BbrBweReceiver : public BweReceiver {
 public:
  explicit BbrBweReceiver(int flow_id);
  virtual ~BbrBweReceiver();
  void ReceivePacket(int64_t arrival_time_ms,
                     const MediaPacket& media_packet) override;
  FeedbackPacket* GetFeedback(int64_t now_ms) override;

 private:
  SimulatedClock clock_;
};
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_BBR_H_
