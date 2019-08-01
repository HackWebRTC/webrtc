/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/report_block_stats.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

#include "test/gtest.h"

namespace webrtc {

class ReportBlockStatsTest : public ::testing::Test {
 protected:
  ReportBlockStatsTest() : kSsrc1(0x12345) {}

  void SetUp() override {
    // kSsrc1: block 1-3.
    block1_1_.packets_lost = 10;
    block1_1_.fraction_lost = 123;
    block1_1_.extended_highest_sequence_number = 24000;
    block1_1_.jitter = 777;
    block1_1_.source_ssrc = kSsrc1;
    block1_2_.packets_lost = 15;
    block1_2_.fraction_lost = 0;
    block1_2_.extended_highest_sequence_number = 24100;
    block1_2_.jitter = 222;
    block1_2_.source_ssrc = kSsrc1;
    block1_3_.packets_lost = 50;
    block1_3_.fraction_lost = 0;
    block1_3_.extended_highest_sequence_number = 24200;
    block1_3_.jitter = 333;
    block1_3_.source_ssrc = kSsrc1;
  }

  RtcpStatistics RtcpReportBlockToRtcpStatistics(const RTCPReportBlock& stats) {
    RtcpStatistics block;
    block.packets_lost = stats.packets_lost;
    block.fraction_lost = stats.fraction_lost;
    block.extended_highest_sequence_number =
        stats.extended_highest_sequence_number;
    block.jitter = stats.jitter;
    return block;
  }

  const uint32_t kSsrc1;
  RTCPReportBlock block1_1_;
  RTCPReportBlock block1_2_;
  RTCPReportBlock block1_3_;
};

TEST_F(ReportBlockStatsTest, StoreAndGetFractionLost) {
  ReportBlockStats stats;
  EXPECT_EQ(-1, stats.FractionLostInPercent());

  // First block.
  stats.Store(kSsrc1, RtcpReportBlockToRtcpStatistics(block1_1_));
  EXPECT_EQ(-1, stats.FractionLostInPercent());
  // fl: 100 * (15-10) / (24100-24000) = 5%
  stats.Store(kSsrc1, RtcpReportBlockToRtcpStatistics(block1_2_));
  EXPECT_EQ(5, stats.FractionLostInPercent());
  // fl: 100 * (50-10) / (24200-24000) = 20%
  stats.Store(kSsrc1, RtcpReportBlockToRtcpStatistics(block1_3_));
  EXPECT_EQ(20, stats.FractionLostInPercent());
}

}  // namespace webrtc
