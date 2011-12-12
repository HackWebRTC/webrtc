/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test_base.h"

using namespace webrtc;
using namespace testing;

class TestErrorObserver : public VoiceEngineObserver {
 public:
  TestErrorObserver() {}
  virtual ~TestErrorObserver() {}
  void CallbackOnError(const int channel, const int error_code) {
    ADD_FAILURE() << "Unexpected error on channel " << channel <<
        ": error code " << error_code;
  }
};

class RtpRtcpBeforeStreamingTest : public TestBase {
 protected:
  TestErrorObserver error_observer_;

  void SetUp();
  void TearDown();

  int channel_;
};

void RtpRtcpBeforeStreamingTest::SetUp() {
#if defined BLACKFIN
  EXPECT_EQ(0, voe_base_->Init(0, LINUX_AUDIO_OSS));
#else
  EXPECT_EQ(0, voe_base_->Init());
#endif

#if defined(WEBRTC_ANDROID)
  EXPECT_EQ(0, voe_hardware_->SetLoudspeakerStatus(false));
#endif

  // Ensure we have an error observer and a channel up.
  EXPECT_EQ(0, voe_base_->RegisterVoiceEngineObserver(error_observer_));
  EXPECT_THAT(channel_ = voe_base_->CreateChannel(), Not(Lt(0)));
}

void RtpRtcpBeforeStreamingTest::TearDown() {
  EXPECT_EQ(0, voe_base_->DeleteChannel(channel_));
  EXPECT_EQ(0, voe_base_->DeRegisterVoiceEngineObserver());
}

TEST_F(RtpRtcpBeforeStreamingTest, MaxNumChannelsIsBiggerThanZero) {
  EXPECT_GT(voe_base_->MaxNumOfChannels(), 0);
}

TEST_F(RtpRtcpBeforeStreamingTest, GetVersionPrintsSomeUsefulInformation) {
  char char_buffer[1024];
  EXPECT_EQ(0, voe_base_->GetVersion(char_buffer));
  EXPECT_THAT(char_buffer, ContainsRegex("VoiceEngine [0-9].[0-9].[0-9]"));
}

TEST_F(RtpRtcpBeforeStreamingTest,
       GetRtcpStatusReturnsTrueByDefaultAndObeysSetRtcpStatus) {
  bool on;
  EXPECT_EQ(0, voe_rtp_rtcp_->GetRTCPStatus(channel_, on));
  EXPECT_TRUE(on);
  EXPECT_EQ(0, voe_rtp_rtcp_->SetRTCPStatus(channel_, false));
  EXPECT_EQ(0, voe_rtp_rtcp_->GetRTCPStatus(channel_, on));
  EXPECT_FALSE(on);
  EXPECT_EQ(0, voe_rtp_rtcp_->SetRTCPStatus(channel_, true));
  EXPECT_EQ(0, voe_rtp_rtcp_->GetRTCPStatus(channel_, on));
  EXPECT_TRUE(on);
}

TEST_F(RtpRtcpBeforeStreamingTest, RtpKeepAliveStatusIsOffByDefault) {
  unsigned char payload_type;
  int delta_seconds;
  bool on;

  // Should be off by default.
  EXPECT_EQ(0, voe_rtp_rtcp_->GetRTPKeepaliveStatus(
      channel_, on, payload_type, delta_seconds));
  EXPECT_FALSE(on);
  EXPECT_EQ(255, payload_type);
  EXPECT_EQ(0, delta_seconds);
}

TEST_F(RtpRtcpBeforeStreamingTest, SetRtpKeepAliveDealsWithInvalidParameters) {
  unsigned char payload_type;
  int delta_seconds;
  bool on;

  // Verify invalid input parameters.
  EXPECT_NE(0, voe_rtp_rtcp_->GetRTPKeepaliveStatus(
      -1, on, payload_type, delta_seconds)) <<
      "Should fail for invalid channel -1.";
  EXPECT_NE(0, voe_rtp_rtcp_->SetRTPKeepaliveStatus(
      -1, true, 0, 15)) <<
      "Should fail for invalid channel -1.";
  EXPECT_NE(0, voe_rtp_rtcp_->SetRTPKeepaliveStatus(
      channel_, true, -1, 15)) <<
      "Should fail for invalid payload -1.";
  EXPECT_NE(0, voe_rtp_rtcp_->SetRTPKeepaliveStatus(
      channel_, true, 0, 61)) <<
      "The delta time must be [1, 60] seconds.";
  EXPECT_EQ(0, voe_rtp_rtcp_->GetRTPKeepaliveStatus(
      channel_, on, payload_type, delta_seconds));
  EXPECT_NE(0, voe_rtp_rtcp_->SetRTPKeepaliveStatus(
      channel_, true, 0));

  // Should still be off, default 0 used by PCMU.
  EXPECT_FALSE(on);
}

TEST_F(RtpRtcpBeforeStreamingTest,
       GetRtpKeepaliveStatusObeysSetRtpKeepaliveStatus) {
  EXPECT_EQ(0, voe_rtp_rtcp_->SetRTCP_CNAME(channel_, "SomeName"));

  // Try valid settings.
  EXPECT_EQ(0, voe_rtp_rtcp_->SetRTPKeepaliveStatus(
      channel_, true, 1));

  unsigned char payload_type;
  int delta_seconds;
  bool on;

  EXPECT_EQ(0, voe_rtp_rtcp_->GetRTPKeepaliveStatus(
      0, on, payload_type, delta_seconds));
  EXPECT_TRUE(on);
  EXPECT_EQ(1, payload_type);
  EXPECT_EQ(15, delta_seconds) << "15 seconds delta is default.";

  // Set the keep-alive payload to 60, which the codecs can't use.
  EXPECT_EQ(0, voe_rtp_rtcp_->SetRTPKeepaliveStatus(
      channel_, true, 60, 3));
  EXPECT_EQ(0, voe_rtp_rtcp_->GetRTPKeepaliveStatus(
      channel_, on, payload_type, delta_seconds));
  EXPECT_TRUE(on);
  EXPECT_EQ(60, payload_type);
  EXPECT_EQ(3, delta_seconds);

  EXPECT_EQ(0, voe_rtp_rtcp_->SetRTPKeepaliveStatus(
      channel_, false, 60));
}

TEST_F(RtpRtcpBeforeStreamingTest, GetLocalSsrcObeysSetLocalSsrc) {
  EXPECT_EQ(0, voe_rtp_rtcp_->SetLocalSSRC(channel_, 1234));
  unsigned int result = 0;
  EXPECT_EQ(0, voe_rtp_rtcp_->GetLocalSSRC(channel_, result));
  EXPECT_EQ(1234u, result);
}
