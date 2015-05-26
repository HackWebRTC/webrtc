/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <queue>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/format_macros.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/voice_engine/test/auto_test/fakes/conference_transport.h"

namespace {
  static const int kRttMs = 25;

  static bool IsNear(int ref, int comp, int error) {
    return (ref - comp <= error) && (comp - ref >= -error);
  }
}

namespace voetest {

TEST(VoeConferenceTest, RttAndStartNtpTime) {
  struct Stats {
    Stats(int64_t rtt_receiver_1, int64_t rtt_receiver_2, int64_t ntp_delay)
        : rtt_receiver_1_(rtt_receiver_1),
          rtt_receiver_2_(rtt_receiver_2),
          ntp_delay_(ntp_delay) {
    }
    int64_t rtt_receiver_1_;
    int64_t rtt_receiver_2_;
    int64_t ntp_delay_;
  };

  const int kDelayMs = 987;
  ConferenceTransport trans;
  trans.SetRtt(kRttMs);

  unsigned int id_1 = trans.AddStream();
  unsigned int id_2 = trans.AddStream();

  EXPECT_TRUE(trans.StartPlayout(id_1));
  // Start NTP time is the time when a stream is played out, rather than
  // when it is added.
  webrtc::SleepMs(kDelayMs);
  EXPECT_TRUE(trans.StartPlayout(id_2));

  const int kMaxRunTimeMs = 25000;
  const int kNeedSuccessivePass = 3;
  const int kStatsRequestIntervalMs = 1000;
  const int kStatsBufferSize = 3;

  uint32 deadline = rtc::TimeAfter(kMaxRunTimeMs);
  // Run the following up to |kMaxRunTimeMs| milliseconds.
  int successive_pass = 0;
  webrtc::CallStatistics stats_1;
  webrtc::CallStatistics stats_2;
  std::queue<Stats> stats_buffer;

  while (rtc::TimeIsLater(rtc::Time(), deadline) &&
      successive_pass < kNeedSuccessivePass) {
    webrtc::SleepMs(kStatsRequestIntervalMs);

    EXPECT_TRUE(trans.GetReceiverStatistics(id_1, &stats_1));
    EXPECT_TRUE(trans.GetReceiverStatistics(id_2, &stats_2));

    // It is not easy to verify the NTP time directly. We verify it by testing
    // the difference of two start NTP times.
    int64_t captured_start_ntp_delay = stats_2.capture_start_ntp_time_ms_ -
        stats_1.capture_start_ntp_time_ms_;

    // For the checks of RTT and start NTP time, We allow 10% accuracy.
    if (IsNear(kRttMs, stats_1.rttMs, kRttMs / 10 + 1) &&
        IsNear(kRttMs, stats_2.rttMs, kRttMs / 10 + 1) &&
        IsNear(kDelayMs, captured_start_ntp_delay, kDelayMs / 10 + 1)) {
      successive_pass++;
    } else {
      successive_pass = 0;
    }
    if (stats_buffer.size() >= kStatsBufferSize) {
      stats_buffer.pop();
    }
    stats_buffer.push(Stats(stats_1.rttMs, stats_2.rttMs,
                            captured_start_ntp_delay));
  }

  EXPECT_GE(successive_pass, kNeedSuccessivePass) << "Expected to get RTT and"
      " start NTP time estimate within 10% of the correct value over "
      << kStatsRequestIntervalMs * kNeedSuccessivePass / 1000
      << " seconds.";
  if (successive_pass < kNeedSuccessivePass) {
    printf("The most recent values (RTT for receiver 1, RTT for receiver 2, "
        "NTP delay between receiver 1 and 2) are (from oldest):\n");
    while (!stats_buffer.empty()) {
      Stats stats = stats_buffer.front();
      printf("(%" PRId64 ", %" PRId64 ", %" PRId64 ")\n", stats.rtt_receiver_1_,
             stats.rtt_receiver_2_, stats.ntp_delay_);
      stats_buffer.pop();
    }
  }
}
}  // namespace voetest
