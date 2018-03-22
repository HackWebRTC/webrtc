/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/mid_oracle.h"

#include "rtc_base/logging.h"
#include "test/gtest.h"

namespace {

using ::webrtc::RTCPReportBlock;
using ::webrtc::MidOracle;

RTCPReportBlock ReportBlockWithSourceSsrc(uint32_t ssrc) {
  RTCPReportBlock report_block;
  report_block.source_ssrc = ssrc;
  return report_block;
}

TEST(MidOracleTest, DoNotSendMidInitially) {
  MidOracle mid_oracle("mid");
  EXPECT_FALSE(mid_oracle.send_mid());
}

TEST(MidOracleTest, SendMidOnceSsrcSet) {
  MidOracle mid_oracle("mid");
  mid_oracle.SetSsrc(52);
  EXPECT_TRUE(mid_oracle.send_mid());
}

TEST(MidOracleTest, IgnoreReportBlockWithUnknownSourceSsrc) {
  MidOracle mid_oracle("mid");
  mid_oracle.SetSsrc(52);
  mid_oracle.OnReceivedRtcpReportBlocks({ReportBlockWithSourceSsrc(63)});
  EXPECT_TRUE(mid_oracle.send_mid());
}

TEST(MidOracleTest, StopSendingMidAfterReceivingRtcpReportWithKnownSourceSsrc) {
  constexpr uint32_t kSsrc = 52;

  MidOracle mid_oracle("mid");
  mid_oracle.SetSsrc(kSsrc);
  mid_oracle.OnReceivedRtcpReportBlocks({ReportBlockWithSourceSsrc(kSsrc)});

  EXPECT_FALSE(mid_oracle.send_mid());
}

TEST(MidOracleTest, RestartSendingMidWhenSsrcChanges) {
  constexpr uint32_t kInitialSsrc = 52;
  constexpr uint32_t kChangedSsrc = 63;

  MidOracle mid_oracle("mid");
  mid_oracle.SetSsrc(kInitialSsrc);
  mid_oracle.OnReceivedRtcpReportBlocks(
      {ReportBlockWithSourceSsrc(kInitialSsrc)});
  mid_oracle.SetSsrc(kChangedSsrc);

  EXPECT_TRUE(mid_oracle.send_mid());
}

}  // namespace
