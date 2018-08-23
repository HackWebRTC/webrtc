/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <memory>
#include <vector>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/audio_coding/codecs/audio_format_conversion.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_receiver_audio.h"
#include "modules/rtp_rtcp/test/testAPI/test_api.h"
#include "rtc_base/rate_limiter.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
const uint16_t kSequenceNumber = 2345;
const uint32_t kSsrc = 3456;
const uint32_t kTimestamp = 4567;

class RtcpCallback : public RtcpIntraFrameObserver {
 public:
  void OnReceivedIntraFrameRequest(uint32_t ssrc) override {}
};

class RtpRtcpRtcpTest : public ::testing::Test {
 protected:
  RtpRtcpRtcpTest()
      : fake_clock_(123456),
        retransmission_rate_limiter_(&fake_clock_, 1000),
        receive_statistics1_(ReceiveStatistics::Create(&fake_clock_)),
        receive_statistics2_(ReceiveStatistics::Create(&fake_clock_)),
        rtp_receiver1_(
            RtpReceiver::CreateAudioReceiver(&fake_clock_,
                                             &receiver_,
                                             &rtp_payload_registry1_)),
        rtp_receiver2_(
            RtpReceiver::CreateAudioReceiver(&fake_clock_,
                                             &receiver_,
                                             &rtp_payload_registry2_)) {}
  ~RtpRtcpRtcpTest() override = default;

  void SetUp() override {
    RtpRtcp::Configuration configuration;
    configuration.audio = true;
    configuration.clock = &fake_clock_;
    configuration.receive_statistics = receive_statistics1_.get();
    configuration.outgoing_transport = &transport1_;
    configuration.intra_frame_callback = &rtcp_callback1_;
    configuration.retransmission_rate_limiter = &retransmission_rate_limiter_;
    module1_.reset(RtpRtcp::CreateRtpRtcp(configuration));

    configuration.receive_statistics = receive_statistics2_.get();
    configuration.outgoing_transport = &transport2_;
    configuration.intra_frame_callback = &rtcp_callback2_;
    module2_.reset(RtpRtcp::CreateRtpRtcp(configuration));

    transport1_.SetSendModule(module2_.get(), &rtp_payload_registry2_,
                              rtp_receiver2_.get(), receive_statistics2_.get());
    transport2_.SetSendModule(module1_.get(), &rtp_payload_registry1_,
                              rtp_receiver1_.get(), receive_statistics1_.get());

    module1_->SetRTCPStatus(RtcpMode::kCompound);
    module2_->SetRTCPStatus(RtcpMode::kCompound);

    module2_->SetSSRC(kSsrc + 1);
    module2_->SetRemoteSSRC(kSsrc);
    module1_->SetSSRC(kSsrc);
    module1_->SetSequenceNumber(kSequenceNumber);
    module1_->SetStartTimestamp(kTimestamp);

    module1_->SetCsrcs(kCsrcs);
    EXPECT_EQ(0, module1_->SetCNAME("john.doe@test.test"));

    EXPECT_EQ(0, module1_->SetSendingStatus(true));

    CodecInst voice_codec;
    voice_codec.pltype = 96;
    voice_codec.plfreq = 8000;
    voice_codec.rate = 64000;
    memcpy(voice_codec.plname, "PCMU", 5);

    EXPECT_EQ(0, module1_->RegisterSendPayload(voice_codec));
    EXPECT_EQ(0, rtp_receiver1_->RegisterReceivePayload(
                     voice_codec.pltype, CodecInstToSdp(voice_codec)));
    EXPECT_EQ(0, module2_->RegisterSendPayload(voice_codec));
    EXPECT_EQ(0, rtp_receiver2_->RegisterReceivePayload(
                     voice_codec.pltype, CodecInstToSdp(voice_codec)));

    // We need to send one RTP packet to get the RTCP packet to be accepted by
    // the receiving module.
    // Send RTP packet with the data "testtest".
    const uint8_t test[9] = "testtest";
    EXPECT_EQ(true,
              module1_->SendOutgoingData(webrtc::kAudioFrameSpeech, 96, 0, -1,
                                         test, 8, nullptr, nullptr, nullptr));
  }

  const std::vector<uint32_t> kCsrcs = {1234, 2345};
  SimulatedClock fake_clock_;
  RateLimiter retransmission_rate_limiter_;
  RtcpCallback rtcp_callback1_;
  RtcpCallback rtcp_callback2_;
  RTPPayloadRegistry rtp_payload_registry1_;
  RTPPayloadRegistry rtp_payload_registry2_;
  TestRtpReceiver receiver_;
  std::unique_ptr<ReceiveStatistics> receive_statistics1_;
  std::unique_ptr<ReceiveStatistics> receive_statistics2_;
  std::unique_ptr<RtpReceiver> rtp_receiver1_;
  std::unique_ptr<RtpReceiver> rtp_receiver2_;
  std::unique_ptr<RtpRtcp> module1_;
  std::unique_ptr<RtpRtcp> module2_;
  LoopBackTransport transport1_;
  LoopBackTransport transport2_;
};

TEST_F(RtpRtcpRtcpTest, RTCP_CNAME) {
  // Set cname of mixed.
  EXPECT_EQ(0, module1_->AddMixedCNAME(kCsrcs[0], "john@192.168.0.1"));
  EXPECT_EQ(0, module1_->AddMixedCNAME(kCsrcs[1], "jane@192.168.0.2"));

  EXPECT_EQ(-1, module1_->RemoveMixedCNAME(kCsrcs[0] + 1));
  EXPECT_EQ(0, module1_->RemoveMixedCNAME(kCsrcs[1]));
  EXPECT_EQ(0, module1_->AddMixedCNAME(kCsrcs[1], "jane@192.168.0.2"));

  // Send RTCP packet, triggered by timer.
  fake_clock_.AdvanceTimeMilliseconds(7500);
  module1_->Process();
  fake_clock_.AdvanceTimeMilliseconds(100);
  module2_->Process();

  char cName[RTCP_CNAME_SIZE];
  EXPECT_EQ(-1, module2_->RemoteCNAME(rtp_receiver2_->SSRC() + 1, cName));

  // Check multiple CNAME.
  EXPECT_EQ(0, module2_->RemoteCNAME(rtp_receiver2_->SSRC(), cName));
  EXPECT_EQ(0, strncmp(cName, "john.doe@test.test", RTCP_CNAME_SIZE));

  EXPECT_EQ(0, module2_->RemoteCNAME(kCsrcs[0], cName));
  EXPECT_EQ(0, strncmp(cName, "john@192.168.0.1", RTCP_CNAME_SIZE));

  EXPECT_EQ(0, module2_->RemoteCNAME(kCsrcs[1], cName));
  EXPECT_EQ(0, strncmp(cName, "jane@192.168.0.2", RTCP_CNAME_SIZE));

  EXPECT_EQ(0, module1_->SetSendingStatus(false));

  // Test that BYE clears the CNAME.
  EXPECT_EQ(-1, module2_->RemoteCNAME(rtp_receiver2_->SSRC(), cName));
}

TEST_F(RtpRtcpRtcpTest, RemoteRTCPStatRemote) {
  std::vector<RTCPReportBlock> report_blocks;
  EXPECT_EQ(0, module1_->RemoteRTCPStat(&report_blocks));
  EXPECT_EQ(0u, report_blocks.size());

  // Send RTCP packet, triggered by timer.
  fake_clock_.AdvanceTimeMilliseconds(7500);
  module1_->Process();
  fake_clock_.AdvanceTimeMilliseconds(100);
  module2_->Process();

  EXPECT_EQ(0, module1_->RemoteRTCPStat(&report_blocks));
  ASSERT_EQ(1u, report_blocks.size());

  // |kSsrc+1| is the SSRC of module2 that send the report.
  EXPECT_EQ(kSsrc + 1, report_blocks[0].sender_ssrc);
  EXPECT_EQ(kSsrc, report_blocks[0].source_ssrc);

  EXPECT_EQ(0, report_blocks[0].packets_lost);
  EXPECT_LT(0u, report_blocks[0].delay_since_last_sender_report);
  EXPECT_EQ(kSequenceNumber, report_blocks[0].extended_highest_sequence_number);
  EXPECT_EQ(0u, report_blocks[0].fraction_lost);
}

}  // namespace
}  // namespace webrtc
