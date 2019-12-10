/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/send_side_bandwidth_estimation.h"

#include "api/rtc_event_log/rtc_event.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_loss_based.h"
#include "logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

MATCHER(LossBasedBweUpdateWithBitrateOnly, "") {
  if (arg->GetType() != RtcEvent::Type::BweUpdateLossBased) {
    return false;
  }
  auto bwe_event = static_cast<RtcEventBweUpdateLossBased*>(arg);
  return bwe_event->bitrate_bps() > 0 && bwe_event->fraction_loss() == 0;
}

MATCHER(LossBasedBweUpdateWithBitrateAndLossFraction, "") {
  if (arg->GetType() != RtcEvent::Type::BweUpdateLossBased) {
    return false;
  }
  auto bwe_event = static_cast<RtcEventBweUpdateLossBased*>(arg);
  return bwe_event->bitrate_bps() > 0 && bwe_event->fraction_loss() > 0;
}

void TestProbing(bool use_delay_based) {
  ::testing::NiceMock<MockRtcEventLog> event_log;
  SendSideBandwidthEstimation bwe(&event_log);
  int64_t now_ms = 0;
  bwe.SetMinMaxBitrate(DataRate::bps(100000), DataRate::bps(1500000));
  bwe.SetSendBitrate(DataRate::bps(200000), Timestamp::ms(now_ms));

  const int kRembBps = 1000000;
  const int kSecondRembBps = kRembBps + 500000;

  bwe.UpdatePacketsLost(/*packets_lost=*/0, /*number_of_packets=*/1,
                        Timestamp::ms(now_ms));
  bwe.UpdateRtt(TimeDelta::ms(50), Timestamp::ms(now_ms));

  // Initial REMB applies immediately.
  if (use_delay_based) {
    bwe.UpdateDelayBasedEstimate(Timestamp::ms(now_ms),
                                 DataRate::bps(kRembBps));
  } else {
    bwe.UpdateReceiverEstimate(Timestamp::ms(now_ms), DataRate::bps(kRembBps));
  }
  bwe.UpdateEstimate(Timestamp::ms(now_ms));
  EXPECT_EQ(kRembBps, bwe.target_rate().bps());

  // Second REMB doesn't apply immediately.
  now_ms += 2001;
  if (use_delay_based) {
    bwe.UpdateDelayBasedEstimate(Timestamp::ms(now_ms),
                                 DataRate::bps(kSecondRembBps));
  } else {
    bwe.UpdateReceiverEstimate(Timestamp::ms(now_ms),
                               DataRate::bps(kSecondRembBps));
  }
  bwe.UpdateEstimate(Timestamp::ms(now_ms));
  EXPECT_EQ(kRembBps, bwe.target_rate().bps());
}

TEST(SendSideBweTest, InitialRembWithProbing) {
  TestProbing(false);
}

TEST(SendSideBweTest, InitialDelayBasedBweWithProbing) {
  TestProbing(true);
}

TEST(SendSideBweTest, SettingSendBitrateOverridesDelayBasedEstimate) {
  ::testing::NiceMock<MockRtcEventLog> event_log;
  SendSideBandwidthEstimation bwe(&event_log);
  static const int kMinBitrateBps = 10000;
  static const int kMaxBitrateBps = 10000000;
  static const int kInitialBitrateBps = 300000;
  static const int kDelayBasedBitrateBps = 350000;
  static const int kForcedHighBitrate = 2500000;

  int64_t now_ms = 0;

  bwe.SetMinMaxBitrate(DataRate::bps(kMinBitrateBps),
                       DataRate::bps(kMaxBitrateBps));
  bwe.SetSendBitrate(DataRate::bps(kInitialBitrateBps), Timestamp::ms(now_ms));

  bwe.UpdateDelayBasedEstimate(Timestamp::ms(now_ms),
                               DataRate::bps(kDelayBasedBitrateBps));
  bwe.UpdateEstimate(Timestamp::ms(now_ms));
  EXPECT_GE(bwe.target_rate().bps(), kInitialBitrateBps);
  EXPECT_LE(bwe.target_rate().bps(), kDelayBasedBitrateBps);

  bwe.SetSendBitrate(DataRate::bps(kForcedHighBitrate), Timestamp::ms(now_ms));
  EXPECT_EQ(bwe.target_rate().bps(), kForcedHighBitrate);
}

}  // namespace webrtc
