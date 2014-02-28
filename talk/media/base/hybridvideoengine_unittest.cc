/*
 * libjingle
 * Copyright 2004 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/base/gunit.h"
#include "talk/media/base/fakemediaengine.h"
#include "talk/media/base/fakenetworkinterface.h"
#include "talk/media/base/fakevideocapturer.h"
#include "talk/media/base/hybridvideoengine.h"
#include "talk/media/base/mediachannel.h"
#include "talk/media/base/testutils.h"

static const cricket::VideoCodec kGenericCodec(97, "Generic", 640, 360, 30, 0);
static const cricket::VideoCodec kVp8Codec(100, "VP8", 640, 360, 30, 0);
static const cricket::VideoCodec kCodecsVp8Only[] = { kVp8Codec };
static const cricket::VideoCodec kCodecsGenericOnly[] = { kGenericCodec };
static const cricket::VideoCodec kCodecsVp8First[] = { kVp8Codec,
                                                       kGenericCodec };
static const cricket::VideoCodec kCodecsGenericFirst[] = { kGenericCodec,
                                                           kVp8Codec };

using cricket::StreamParams;

class FakeVp8VideoEngine : public cricket::FakeVideoEngine {
 public:
  FakeVp8VideoEngine() {
    SetCodecs(MAKE_VECTOR(kCodecsVp8Only));
  }
};
class FakeGenericVideoEngine : public cricket::FakeVideoEngine {
 public:
  FakeGenericVideoEngine() {
    SetCodecs(MAKE_VECTOR(kCodecsGenericOnly));
  }

  // For testing purposes, mimic the behavior of a media engine that throws out
  // resolutions that don't match the codec list. A width or height of 0
  // trivially will never match the codec list, so this is sufficient for
  // testing the case we want (0x0).
  virtual bool FindCodec(const cricket::VideoCodec& codec) {
    if (codec.width == 0 || codec.height == 0) {
      return false;
    } else {
      return cricket::FakeVideoEngine::FindCodec(codec);
    }
  }
};
class HybridVideoEngineForTest : public cricket::HybridVideoEngine<
    FakeVp8VideoEngine, FakeGenericVideoEngine> {
 public:
  HybridVideoEngineForTest()
      :
      num_ch1_send_on_(0),
      num_ch1_send_off_(0),
      send_width_(0),
      send_height_(0) { }
  cricket::FakeVideoEngine* sub_engine1() { return &video1_; }
  cricket::FakeVideoEngine* sub_engine2() { return &video2_; }

  // From base class HybridVideoEngine.
  void OnSendChange1(cricket::VideoMediaChannel* channel1, bool send) {
    if (send) {
      ++num_ch1_send_on_;
    } else {
      ++num_ch1_send_off_;
    }
  }
  // From base class HybridVideoEngine
  void OnNewSendResolution(int width, int height) {
    send_width_ = width;
    send_height_ = height;
  }

  int num_ch1_send_on() const { return num_ch1_send_on_; }
  int num_ch1_send_off() const { return num_ch1_send_off_; }

  int send_width() const { return send_width_; }
  int send_height() const { return send_height_; }

 private:
  int num_ch1_send_on_;
  int num_ch1_send_off_;

  int send_width_;
  int send_height_;
};

class HybridVideoEngineTest : public testing::Test {
 public:
  HybridVideoEngineTest() : sub_channel1_(NULL), sub_channel2_(NULL) {
  }
  ~HybridVideoEngineTest() {
    engine_.Terminate();
  }
  bool SetupEngine() {
    bool result = engine_.Init(talk_base::Thread::Current());
    if (result) {
      channel_.reset(engine_.CreateChannel(NULL));
      result = (channel_.get() != NULL);
      sub_channel1_ = engine_.sub_engine1()->GetChannel(0);
      sub_channel2_ = engine_.sub_engine2()->GetChannel(0);
    }
    return result;
  }
  bool SetupRenderAndAddStream(const StreamParams& sp) {
    if (!SetupEngine())
      return false;
    channel_->SetInterface(transport_.get());
    return channel_->SetRecvCodecs(engine_.codecs()) &&
        channel_->AddSendStream(sp) &&
        channel_->SetRender(true);
  }
  void DeliverPacket(const void* data, int len) {
    talk_base::Buffer packet(data, len);
    channel_->OnPacketReceived(&packet, talk_base::CreatePacketTime(0));
  }
  void DeliverRtcp(const void* data, int len) {
    talk_base::Buffer packet(data, len);
    channel_->OnRtcpReceived(&packet, talk_base::CreatePacketTime(0));
  }

 protected:
  void TestSetSendCodecs(cricket::FakeVideoEngine* sub_engine,
                         const std::vector<cricket::VideoCodec>& codecs) {
    EXPECT_TRUE(SetupRenderAndAddStream(StreamParams::CreateLegacy(1234)));
    EXPECT_TRUE(channel_->SetSendCodecs(codecs));
    cricket::FakeVideoMediaChannel* sub_channel = sub_engine->GetChannel(0);
    ASSERT_EQ(1U, sub_channel->send_codecs().size());
    EXPECT_EQ(codecs[0], sub_channel->send_codecs()[0]);
    EXPECT_TRUE(channel_->SetSend(true));
    EXPECT_TRUE(sub_channel->sending());
  }
  void TestSetSendBandwidth(cricket::FakeVideoEngine* sub_engine,
                            const std::vector<cricket::VideoCodec>& codecs,
                            int start_bitrate,
                            int max_bitrate) {
    EXPECT_TRUE(SetupRenderAndAddStream(StreamParams::CreateLegacy(1234)));
    EXPECT_TRUE(channel_->SetSendCodecs(codecs));
    EXPECT_TRUE(channel_->SetStartSendBandwidth(start_bitrate));
    EXPECT_TRUE(channel_->SetMaxSendBandwidth(max_bitrate));
    cricket::FakeVideoMediaChannel* sub_channel = sub_engine->GetChannel(0);
    EXPECT_EQ(start_bitrate, sub_channel->start_bps());
    EXPECT_EQ(max_bitrate, sub_channel->max_bps());
  }
  HybridVideoEngineForTest engine_;
  talk_base::scoped_ptr<cricket::HybridVideoMediaChannel> channel_;
  talk_base::scoped_ptr<cricket::FakeNetworkInterface> transport_;
  cricket::FakeVideoMediaChannel* sub_channel1_;
  cricket::FakeVideoMediaChannel* sub_channel2_;
};

TEST_F(HybridVideoEngineTest, StartupShutdown) {
  EXPECT_TRUE(engine_.Init(talk_base::Thread::Current()));
  engine_.Terminate();
}

// Tests that SetDefaultVideoEncoderConfig passes down to both engines.
TEST_F(HybridVideoEngineTest, SetDefaultVideoEncoderConfig) {
  cricket::VideoEncoderConfig config(
      cricket::VideoCodec(105, "", 640, 400, 30, 0), 1, 2);
  EXPECT_TRUE(engine_.SetDefaultEncoderConfig(config));

  cricket::VideoEncoderConfig config_1 = config;
  config_1.max_codec.name = kCodecsVp8Only[0].name;
  EXPECT_EQ(config_1, engine_.sub_engine1()->default_encoder_config());

  cricket::VideoEncoderConfig config_2 = config;
  config_2.max_codec.name = kCodecsGenericOnly[0].name;
  EXPECT_EQ(config_2, engine_.sub_engine2()->default_encoder_config());
}

// Tests that GetDefaultVideoEncoderConfig picks a meaningful encoder config
// based on the underlying engine config and then after a call to
// SetDefaultEncoderConfig on the hybrid engine.
TEST_F(HybridVideoEngineTest, SetDefaultVideoEncoderConfigDefaultValue) {
  cricket::VideoEncoderConfig blank_config;
  cricket::VideoEncoderConfig meaningful_config1(
      cricket::VideoCodec(111, "abcd", 320, 240, 30, 0), 1, 2);
  cricket::VideoEncoderConfig meaningful_config2(
      cricket::VideoCodec(111, "abcd", 1280, 720, 30, 0), 1, 2);
  cricket::VideoEncoderConfig meaningful_config3(
      cricket::VideoCodec(111, "abcd", 640, 360, 30, 0), 1, 2);
  engine_.sub_engine1()->SetDefaultEncoderConfig(blank_config);
  engine_.sub_engine2()->SetDefaultEncoderConfig(blank_config);
  EXPECT_EQ(blank_config, engine_.GetDefaultEncoderConfig());

  engine_.sub_engine2()->SetDefaultEncoderConfig(meaningful_config2);
  EXPECT_EQ(meaningful_config2, engine_.GetDefaultEncoderConfig());

  engine_.sub_engine1()->SetDefaultEncoderConfig(meaningful_config1);
  EXPECT_EQ(meaningful_config1, engine_.GetDefaultEncoderConfig());

  EXPECT_TRUE(engine_.SetDefaultEncoderConfig(meaningful_config3));
  // The overall config should now match, though the codec name will have been
  // rewritten for the first media engine.
  meaningful_config3.max_codec.name = kCodecsVp8Only[0].name;
  EXPECT_EQ(meaningful_config3, engine_.GetDefaultEncoderConfig());
}

// Tests that our engine has the right codecs in the right order.
TEST_F(HybridVideoEngineTest, CheckCodecs) {
  const std::vector<cricket::VideoCodec>& c = engine_.codecs();
  ASSERT_EQ(2U, c.size());
  EXPECT_EQ(kVp8Codec, c[0]);
  EXPECT_EQ(kGenericCodec, c[1]);
}

// Tests that our engine has the right caps.
TEST_F(HybridVideoEngineTest, CheckCaps) {
  EXPECT_EQ(cricket::VIDEO_SEND | cricket::VIDEO_RECV,
      engine_.GetCapabilities());
}

// Tests that we can create and destroy a channel.
TEST_F(HybridVideoEngineTest, CreateChannel) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(sub_channel1_ != NULL);
  EXPECT_TRUE(sub_channel2_ != NULL);
}

// Tests that we properly handle failures in CreateChannel.
TEST_F(HybridVideoEngineTest, CreateChannelFail) {
  engine_.sub_engine1()->set_fail_create_channel(true);
  EXPECT_FALSE(SetupEngine());
  EXPECT_TRUE(channel_.get() == NULL);
  EXPECT_TRUE(sub_channel1_ == NULL);
  EXPECT_TRUE(sub_channel2_ == NULL);
  engine_.sub_engine1()->set_fail_create_channel(false);
  engine_.sub_engine2()->set_fail_create_channel(true);
  EXPECT_FALSE(SetupEngine());
  EXPECT_TRUE(channel_.get() == NULL);
  EXPECT_TRUE(sub_channel1_ == NULL);
  EXPECT_TRUE(sub_channel2_ == NULL);
}

// Test that we set our inbound codecs and settings properly.
TEST_F(HybridVideoEngineTest, SetLocalDescription) {
  EXPECT_TRUE(SetupEngine());
  channel_->SetInterface(transport_.get());
  EXPECT_TRUE(channel_->SetRecvCodecs(engine_.codecs()));
  ASSERT_EQ(1U, sub_channel1_->recv_codecs().size());
  ASSERT_EQ(1U, sub_channel2_->recv_codecs().size());
  EXPECT_EQ(kVp8Codec, sub_channel1_->recv_codecs()[0]);
  EXPECT_EQ(kGenericCodec, sub_channel2_->recv_codecs()[0]);
  StreamParams stream;
  stream.id = "TestStream";
  stream.ssrcs.push_back(1234);
  stream.cname = "5678";
  EXPECT_TRUE(channel_->AddSendStream(stream));
  EXPECT_EQ(1234U, sub_channel1_->send_ssrc());
  EXPECT_EQ(1234U, sub_channel2_->send_ssrc());
  EXPECT_EQ("5678", sub_channel1_->rtcp_cname());
  EXPECT_EQ("5678", sub_channel2_->rtcp_cname());
  EXPECT_TRUE(channel_->SetRender(true));
  // We've called SetRender, so we should be playing out, but not yet sending.
  EXPECT_TRUE(sub_channel1_->playout());
  EXPECT_TRUE(sub_channel2_->playout());
  EXPECT_FALSE(sub_channel1_->sending());
  EXPECT_FALSE(sub_channel2_->sending());
  // We may get SetSend(false) calls during call setup.
  // Since this causes no change in state, they should no-op and return true.
  EXPECT_TRUE(channel_->SetSend(false));
  EXPECT_FALSE(sub_channel1_->sending());
  EXPECT_FALSE(sub_channel2_->sending());
}

TEST_F(HybridVideoEngineTest, OnNewSendResolution) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->SetSendCodecs(MAKE_VECTOR(kCodecsVp8First)));
  EXPECT_EQ(640, engine_.send_width());
  EXPECT_EQ(360, engine_.send_height());
}

// Test that we converge to the active channel for engine 1.
TEST_F(HybridVideoEngineTest, SetSendCodecs1) {
  // This will nuke the object that sub_channel2_ points to.
  TestSetSendCodecs(engine_.sub_engine1(), MAKE_VECTOR(kCodecsVp8First));
  EXPECT_TRUE(engine_.sub_engine2()->GetChannel(0) == NULL);
}

// Test that we converge to the active channel for engine 2.
TEST_F(HybridVideoEngineTest, SetSendCodecs2) {
  // This will nuke the object that sub_channel1_ points to.
  TestSetSendCodecs(engine_.sub_engine2(), MAKE_VECTOR(kCodecsGenericFirst));
  EXPECT_TRUE(engine_.sub_engine1()->GetChannel(0) == NULL);
}

// Test that we don't accidentally eat 0x0 in SetSendCodecs
TEST_F(HybridVideoEngineTest, SetSendCodecs0x0) {
  EXPECT_TRUE(SetupRenderAndAddStream(StreamParams::CreateLegacy(1234)));
  // Send using generic codec, but with 0x0 resolution.
  std::vector<cricket::VideoCodec> codecs(MAKE_VECTOR(kCodecsGenericFirst));
  codecs.resize(1);
  codecs[0].width = 0;
  codecs[0].height = 0;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
}

// Test setting the send bandwidth for VP8.
TEST_F(HybridVideoEngineTest, SetSendBandwidth1) {
  TestSetSendBandwidth(engine_.sub_engine1(),
                       MAKE_VECTOR(kCodecsVp8First),
                       100000,
                       384000);
}

// Test setting the send bandwidth for a generic codec.
TEST_F(HybridVideoEngineTest, SetSendBandwidth2) {
  TestSetSendBandwidth(engine_.sub_engine2(),
                       MAKE_VECTOR(kCodecsGenericFirst),
                       100001,
                       384002);
}

// Test that we dump RTP packets that arrive early.
TEST_F(HybridVideoEngineTest, HandleEarlyRtp) {
  static const uint8 kPacket[1024] = { 0 };
  static const uint8 kRtcp[1024] = { 1 };
  EXPECT_TRUE(SetupRenderAndAddStream(StreamParams::CreateLegacy(1234)));
  DeliverPacket(kPacket, sizeof(kPacket));
  DeliverRtcp(kRtcp, sizeof(kRtcp));
  EXPECT_TRUE(sub_channel1_->CheckNoRtp());
  EXPECT_TRUE(sub_channel2_->CheckNoRtp());
  EXPECT_TRUE(sub_channel1_->CheckNoRtcp());
  EXPECT_TRUE(sub_channel2_->CheckNoRtcp());
}

// Test that we properly pass on normal RTP packets.
TEST_F(HybridVideoEngineTest, HandleRtp) {
  static const uint8 kPacket[1024] = { 0 };
  static const uint8 kRtcp[1024] = { 1 };
  EXPECT_TRUE(SetupRenderAndAddStream(StreamParams::CreateLegacy(1234)));
  EXPECT_TRUE(channel_->SetSendCodecs(MAKE_VECTOR(kCodecsVp8First)));
  EXPECT_TRUE(channel_->SetSend(true));
  DeliverPacket(kPacket, sizeof(kPacket));
  DeliverRtcp(kRtcp, sizeof(kRtcp));
  EXPECT_TRUE(sub_channel1_->CheckRtp(kPacket, sizeof(kPacket)));
  EXPECT_TRUE(sub_channel1_->CheckRtcp(kRtcp, sizeof(kRtcp)));
}

// Test that we properly connect media error signal.
TEST_F(HybridVideoEngineTest, MediaErrorSignal) {
  cricket::VideoMediaErrorCatcher catcher;

  // Verify no signal from either channel before the active channel is set.
  EXPECT_TRUE(SetupEngine());
  channel_->SignalMediaError.connect(&catcher,
      &cricket::VideoMediaErrorCatcher::OnError);
  sub_channel1_->SignalMediaError(1, cricket::VideoMediaChannel::ERROR_OTHER);
  EXPECT_EQ(0U, catcher.ssrc());
  sub_channel2_->SignalMediaError(2,
      cricket::VideoMediaChannel::ERROR_REC_DEVICE_OPEN_FAILED);
  EXPECT_EQ(0U, catcher.ssrc());

  // Set vp8 as active channel and verify that a signal comes from it.
  EXPECT_TRUE(channel_->SetSendCodecs(MAKE_VECTOR(kCodecsVp8First)));
  sub_channel1_->SignalMediaError(1, cricket::VideoMediaChannel::ERROR_OTHER);
  EXPECT_EQ(cricket::VideoMediaChannel::ERROR_OTHER, catcher.error());
  EXPECT_EQ(1U, catcher.ssrc());

  // Set generic codec as active channel and verify that a signal comes from it.
  EXPECT_TRUE(SetupEngine());
  channel_->SignalMediaError.connect(&catcher,
      &cricket::VideoMediaErrorCatcher::OnError);
  EXPECT_TRUE(channel_->SetSendCodecs(MAKE_VECTOR(kCodecsGenericFirst)));
  sub_channel2_->SignalMediaError(2,
      cricket::VideoMediaChannel::ERROR_REC_DEVICE_OPEN_FAILED);
  EXPECT_EQ(cricket::VideoMediaChannel::ERROR_REC_DEVICE_OPEN_FAILED,
      catcher.error());
  EXPECT_EQ(2U, catcher.ssrc());
}

// Test that SetSend doesn't re-enter.
TEST_F(HybridVideoEngineTest, RepeatSetSend) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->SetSendCodecs(MAKE_VECTOR(kCodecsVp8First)));

  // Verify initial status.
  EXPECT_FALSE(channel_->sending());
  EXPECT_FALSE(sub_channel1_->sending());
  EXPECT_EQ(0, engine_.num_ch1_send_on());
  EXPECT_EQ(0, engine_.num_ch1_send_off());

  // Verfiy SetSend(true) works correctly.
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_TRUE(channel_->sending());
  EXPECT_TRUE(sub_channel1_->sending());
  EXPECT_EQ(1, engine_.num_ch1_send_on());
  EXPECT_EQ(0, engine_.num_ch1_send_off());

  // SetSend(true) again and verify nothing changes.
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_TRUE(channel_->sending());
  EXPECT_TRUE(sub_channel1_->sending());
  EXPECT_EQ(1, engine_.num_ch1_send_on());
  EXPECT_EQ(0, engine_.num_ch1_send_off());

  // Verify SetSend(false) works correctly.
  EXPECT_TRUE(channel_->SetSend(false));
  EXPECT_FALSE(channel_->sending());
  EXPECT_FALSE(sub_channel1_->sending());
  EXPECT_EQ(1, engine_.num_ch1_send_on());
  EXPECT_EQ(1, engine_.num_ch1_send_off());

  // SetSend(false) again and verfiy nothing changes.
  EXPECT_TRUE(channel_->SetSend(false));
  EXPECT_FALSE(channel_->sending());
  EXPECT_FALSE(sub_channel1_->sending());
  EXPECT_EQ(1, engine_.num_ch1_send_on());
  EXPECT_EQ(1, engine_.num_ch1_send_off());
}

// Test that SetOptions.
TEST_F(HybridVideoEngineTest, SetOptions) {
  cricket::VideoOptions vmo;
  vmo.video_high_bitrate.Set(true);
  vmo.system_low_adaptation_threshhold.Set(0.10f);
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->SetOptions(vmo));

  bool high_bitrate;
  float low;
  EXPECT_TRUE(sub_channel1_->GetOptions(&vmo));
  EXPECT_TRUE(vmo.video_high_bitrate.Get(&high_bitrate));
  EXPECT_TRUE(high_bitrate);
  EXPECT_TRUE(vmo.system_low_adaptation_threshhold.Get(&low));
  EXPECT_EQ(0.10f, low);
  EXPECT_TRUE(sub_channel2_->GetOptions(&vmo));
  EXPECT_TRUE(vmo.video_high_bitrate.Get(&high_bitrate));
  EXPECT_TRUE(high_bitrate);
  EXPECT_TRUE(vmo.system_low_adaptation_threshhold.Get(&low));
  EXPECT_EQ(0.10f, low);

  vmo.video_high_bitrate.Set(false);
  vmo.system_low_adaptation_threshhold.Set(0.50f);

  EXPECT_TRUE(channel_->SetOptions(vmo));
  EXPECT_TRUE(sub_channel1_->GetOptions(&vmo));
  EXPECT_TRUE(vmo.video_high_bitrate.Get(&high_bitrate));
  EXPECT_FALSE(high_bitrate);
  EXPECT_TRUE(vmo.system_low_adaptation_threshhold.Get(&low));
  EXPECT_EQ(0.50f, low);
  EXPECT_TRUE(sub_channel2_->GetOptions(&vmo));
  EXPECT_TRUE(vmo.video_high_bitrate.Get(&high_bitrate));
  EXPECT_FALSE(high_bitrate);
  EXPECT_TRUE(vmo.system_low_adaptation_threshhold.Get(&low));
  EXPECT_EQ(0.50f, low);
}

TEST_F(HybridVideoEngineTest, SetCapturer) {
  EXPECT_TRUE(SetupEngine());
  // Set vp8 as active channel and verify that capturer can be set.
  EXPECT_TRUE(channel_->SetSendCodecs(MAKE_VECTOR(kCodecsVp8First)));
  cricket::FakeVideoCapturer fake_video_capturer;
  EXPECT_TRUE(channel_->SetCapturer(0, &fake_video_capturer));
  EXPECT_TRUE(channel_->SetCapturer(0, NULL));

  // Set generic codec active channel and verify that capturer can be set.
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->SetSendCodecs(MAKE_VECTOR(kCodecsGenericFirst)));
  EXPECT_TRUE(channel_->SetCapturer(0, &fake_video_capturer));
  EXPECT_TRUE(channel_->SetCapturer(0, NULL));
}
