/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/system_wrappers/interface/atomic32.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/voice_engine/test/auto_test/fixtures/after_streaming_fixture.h"

class ExtensionVerifyTransport : public webrtc::Transport {
 public:
  ExtensionVerifyTransport()
      : received_packets_(0),
        ok_packets_(0),
        parser_(webrtc::RtpHeaderParser::Create()),
        audio_level_id_(-1),
        absolute_sender_time_id_(-1) {
  }

  virtual int SendPacket(int channel, const void* data, int len) {
    ++received_packets_;
    webrtc::RTPHeader header = {0};
    if (parser_->Parse(static_cast<const uint8_t*>(data), len, &header)) {
      bool ok = true;
      if (audio_level_id_ >= 0 && !header.extension.hasAudioLevel) {
        ok = false;
      }
      if (absolute_sender_time_id_ >= 0 &&
          !header.extension.hasAbsoluteSendTime) {
        ok = false;
      }
      if (ok) {
        ++ok_packets_;
      }
    }
    return len;
  }

  virtual int SendRTCPPacket(int channel, const void* data, int len) {
    return len;
  }

  void SetAudioLevelId(int id) {
    audio_level_id_ = id;
    parser_->RegisterRtpHeaderExtension(webrtc::kRtpExtensionAudioLevel, id);
  }

  void SetAbsoluteSenderTimeId(int id) {
    absolute_sender_time_id_ = id;
    parser_->RegisterRtpHeaderExtension(webrtc::kRtpExtensionAbsoluteSendTime,
                                        id);
  }

  bool WaitForNPackets(int count) {
    while (received_packets_.Value() < count) {
      webrtc::SleepMs(10);
    }
    return (ok_packets_.Value() == count);
  }

 private:
  webrtc::Atomic32 received_packets_;
  webrtc::Atomic32 ok_packets_;
  webrtc::scoped_ptr<webrtc::RtpHeaderParser> parser_;
  int audio_level_id_;
  int absolute_sender_time_id_;
};

class SendRtpRtcpHeaderExtensionsTest : public AfterStreamingFixture {
 protected:
  virtual void SetUp() {
    PausePlaying();
    EXPECT_EQ(0, voe_network_->DeRegisterExternalTransport(channel_));
    EXPECT_EQ(0, voe_network_->RegisterExternalTransport(channel_,
                                                         verifying_transport_));
  }
  virtual void TearDown() {
    PausePlaying();
  }

  ExtensionVerifyTransport verifying_transport_;
};

TEST_F(SendRtpRtcpHeaderExtensionsTest, SentPacketsIncludeAudioLevel) {
  EXPECT_EQ(0, voe_rtp_rtcp_->SetSendAudioLevelIndicationStatus(channel_, true,
                                                                9));
  verifying_transport_.SetAudioLevelId(9);
  ResumePlaying();
  EXPECT_TRUE(verifying_transport_.WaitForNPackets(10));
}

TEST_F(SendRtpRtcpHeaderExtensionsTest, SentPacketsIncludeAbsoluteSenderTime) {
  EXPECT_EQ(0, voe_rtp_rtcp_->SetSendAbsoluteSenderTimeStatus(channel_, true,
                                                              11));
  verifying_transport_.SetAbsoluteSenderTimeId(11);
  ResumePlaying();
  EXPECT_TRUE(verifying_transport_.WaitForNPackets(10));
}

TEST_F(SendRtpRtcpHeaderExtensionsTest, SentPacketsIncludeAllExtensions1) {
  EXPECT_EQ(0, voe_rtp_rtcp_->SetSendAudioLevelIndicationStatus(channel_, true,
                                                                9));
  EXPECT_EQ(0, voe_rtp_rtcp_->SetSendAbsoluteSenderTimeStatus(channel_, true,
                                                              11));
  verifying_transport_.SetAudioLevelId(9);
  verifying_transport_.SetAbsoluteSenderTimeId(11);
  ResumePlaying();
  EXPECT_TRUE(verifying_transport_.WaitForNPackets(10));
}

TEST_F(SendRtpRtcpHeaderExtensionsTest, SentPacketsIncludeAllExtensions2) {
  EXPECT_EQ(0, voe_rtp_rtcp_->SetSendAbsoluteSenderTimeStatus(channel_, true,
                                                              3));
  EXPECT_EQ(0, voe_rtp_rtcp_->SetSendAudioLevelIndicationStatus(channel_, true,
                                                                9));
  verifying_transport_.SetAbsoluteSenderTimeId(3);
  verifying_transport_.SetAudioLevelId(9);
  ResumePlaying();
  EXPECT_TRUE(verifying_transport_.WaitForNPackets(10));
}

class ReceiveRtpRtcpHeaderExtensionsTest : public AfterStreamingFixture {
 protected:
  virtual void SetUp() {
    PausePlaying();
  }
};

TEST_F(ReceiveRtpRtcpHeaderExtensionsTest, ReceivedAbsoluteSenderTimeWorks) {
  EXPECT_EQ(0, voe_rtp_rtcp_->SetSendAbsoluteSenderTimeStatus(channel_, true,
                                                              11));
  EXPECT_EQ(0, voe_rtp_rtcp_->SetReceiveAbsoluteSenderTimeStatus(channel_, true,
                                                              11));
  ResumePlaying();

  // Ensure the RTP-RTCP process gets scheduled.
  Sleep(1000);

  // TODO(solenberg): Verify received packets are forwarded to RBE.
}
