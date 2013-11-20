/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.h"

namespace webrtc {
namespace {

class RtcpRttStatsTestImpl : public RtcpRttStats {
 public:
  RtcpRttStatsTestImpl() : rtt_ms_(0) {}
  virtual ~RtcpRttStatsTestImpl() {}

  virtual void OnRttUpdate(uint32_t rtt_ms) {
    rtt_ms_ = rtt_ms;
  }
  virtual uint32_t LastProcessedRtt() const {
    return rtt_ms_;
  }
  uint32_t rtt_ms_;
};

class SendTransport : public Transport,
                      public NullRtpData {
 public:
  SendTransport() : rtp_rtcp_impl_(NULL), clock_(NULL), delay_ms_(0) {}

  void SetRtpRtcpModule(ModuleRtpRtcpImpl* rtp_rtcp_impl) {
    rtp_rtcp_impl_ = rtp_rtcp_impl;
  }
  void SimulateNetworkDelay(int delay_ms, SimulatedClock* clock) {
    clock_ = clock;
    delay_ms_ = delay_ms;
  }
  virtual int SendPacket(int /*ch*/, const void* /*data*/, int /*len*/) {
    return -1;
  }
  virtual int SendRTCPPacket(int /*ch*/, const void *data, int len) {
    if (clock_) {
      clock_->AdvanceTimeMilliseconds(delay_ms_);
    }
    EXPECT_TRUE(rtp_rtcp_impl_ != NULL);
    EXPECT_EQ(0, rtp_rtcp_impl_->IncomingRtcpPacket(
        static_cast<const uint8_t*>(data), len));
    return len;
  }
  ModuleRtpRtcpImpl* rtp_rtcp_impl_;
  SimulatedClock* clock_;
  int delay_ms_;
};
}  // namespace

class RtpRtcpImplTest : public ::testing::Test {
 protected:
  RtpRtcpImplTest()
      : clock_(1335900000),
        receive_statistics_(ReceiveStatistics::Create(&clock_)) {
    RtpRtcp::Configuration configuration;
    configuration.id = 0;
    configuration.audio = false;
    configuration.clock = &clock_;
    configuration.outgoing_transport = &transport_;
    configuration.receive_statistics = receive_statistics_.get();
    configuration.rtt_stats = &rtt_stats_;

    rtp_rtcp_impl_.reset(new ModuleRtpRtcpImpl(configuration));
    transport_.SetRtpRtcpModule(rtp_rtcp_impl_.get());
  }

  SimulatedClock clock_;
  scoped_ptr<ReceiveStatistics> receive_statistics_;
  scoped_ptr<ModuleRtpRtcpImpl> rtp_rtcp_impl_;
  SendTransport transport_;
  RtcpRttStatsTestImpl rtt_stats_;
};

TEST_F(RtpRtcpImplTest, Rtt) {
  const uint32_t kSsrc = 0x12345;
  RTPHeader header = {};
  header.timestamp = 1;
  header.sequenceNumber = 123;
  header.ssrc = kSsrc;
  header.headerLength = 12;
  receive_statistics_->IncomingPacket(header, 100, false);

  rtp_rtcp_impl_->SetRemoteSSRC(kSsrc);
  EXPECT_EQ(0, rtp_rtcp_impl_->SetSendingStatus(true));
  EXPECT_EQ(0, rtp_rtcp_impl_->SetRTCPStatus(kRtcpCompound));
  EXPECT_EQ(0, rtp_rtcp_impl_->SetSSRC(kSsrc));

  // A SR should have been sent and received.
  EXPECT_EQ(0, rtp_rtcp_impl_->SendRTCP(kRtcpReport));

  // Send new SR. A response to the last SR should be sent.
  clock_.AdvanceTimeMilliseconds(1000);
  transport_.SimulateNetworkDelay(100, &clock_);
  EXPECT_EQ(0, rtp_rtcp_impl_->SendRTCP(kRtcpReport));

  // Verify RTT.
  uint16_t rtt;
  uint16_t avg_rtt;
  uint16_t min_rtt;
  uint16_t max_rtt;
  EXPECT_EQ(0, rtp_rtcp_impl_->RTT(kSsrc, &rtt, &avg_rtt, &min_rtt, &max_rtt));
  EXPECT_EQ(100, rtt);
  EXPECT_EQ(100, avg_rtt);
  EXPECT_EQ(100, min_rtt);
  EXPECT_EQ(100, max_rtt);

  // No RTT from other ssrc.
  EXPECT_EQ(-1,
      rtp_rtcp_impl_->RTT(kSsrc + 1, &rtt, &avg_rtt, &min_rtt, &max_rtt));
}

TEST_F(RtpRtcpImplTest, RttForReceiverOnly) {
  rtp_rtcp_impl_->SetRtcpXrRrtrStatus(true);
  EXPECT_EQ(0, rtp_rtcp_impl_->SetSendingStatus(false));
  EXPECT_EQ(0, rtp_rtcp_impl_->SetRTCPStatus(kRtcpCompound));
  EXPECT_EQ(0, rtp_rtcp_impl_->SetSSRC(0x12345));

  // A Receiver time reference report (RTRR) should be sent and received.
  EXPECT_EQ(0, rtp_rtcp_impl_->SendRTCP(kRtcpReport));

  // Send new RTRR. A response to the last RTRR should be sent.
  clock_.AdvanceTimeMilliseconds(1000);
  transport_.SimulateNetworkDelay(100, &clock_);
  EXPECT_EQ(0, rtp_rtcp_impl_->SendRTCP(kRtcpReport));

  // Verify RTT.
  EXPECT_EQ(0U, rtt_stats_.LastProcessedRtt());
  EXPECT_EQ(0U, rtp_rtcp_impl_->rtt_ms());

  rtp_rtcp_impl_->Process();
  EXPECT_EQ(100U, rtt_stats_.LastProcessedRtt());
  EXPECT_EQ(100U, rtp_rtcp_impl_->rtt_ms());
}

}  // namespace webrtc
