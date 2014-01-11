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

#include "talk/base/fakecpumonitor.h"
#include "talk/base/gunit.h"
#include "talk/base/logging.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/stream.h"
#include "talk/media/base/constants.h"
#include "talk/media/base/fakemediaprocessor.h"
#include "talk/media/base/fakenetworkinterface.h"
#include "talk/media/base/fakevideorenderer.h"
#include "talk/media/base/mediachannel.h"
#include "talk/media/base/testutils.h"
#include "talk/media/base/videoengine_unittest.h"
#include "talk/media/webrtc/fakewebrtcvideocapturemodule.h"
#include "talk/media/webrtc/fakewebrtcvideoengine.h"
#include "talk/media/webrtc/fakewebrtcvoiceengine.h"
#include "talk/media/webrtc/webrtcvideocapturer.h"
#include "talk/media/webrtc/webrtcvideoengine.h"
#include "talk/media/webrtc/webrtcvideoframe.h"
#include "talk/media/webrtc/webrtcvoiceengine.h"
#include "talk/session/media/mediasession.h"
#include "webrtc/system_wrappers/interface/trace.h"

// Tests for the WebRtcVideoEngine/VideoChannel code.

static const cricket::VideoCodec kVP8Codec720p(100, "VP8", 1280, 720, 30, 0);
static const cricket::VideoCodec kVP8Codec360p(100, "VP8", 640, 360, 30, 0);
static const cricket::VideoCodec kVP8Codec270p(100, "VP8", 480, 270, 30, 0);
static const cricket::VideoCodec kVP8Codec180p(100, "VP8", 320, 180, 30, 0);

static const cricket::VideoCodec kVP8Codec(100, "VP8", 640, 400, 30, 0);
static const cricket::VideoCodec kRedCodec(101, "red", 0, 0, 0, 0);
static const cricket::VideoCodec kUlpFecCodec(102, "ulpfec", 0, 0, 0, 0);
static const cricket::VideoCodec* const kVideoCodecs[] = {
    &kVP8Codec,
    &kRedCodec,
    &kUlpFecCodec
};

static const unsigned int kMinBandwidthKbps = 50;
static const unsigned int kStartBandwidthKbps = 300;
static const unsigned int kMaxBandwidthKbps = 2000;

static const unsigned int kNumberOfTemporalLayers = 1;

static const uint32 kSsrcs1[] = {1};
static const uint32 kSsrcs2[] = {1, 2};
static const uint32 kSsrcs3[] = {1, 2, 3};
static const uint32 kRtxSsrc1[] = {4};
static const uint32 kRtxSsrcs3[] = {4, 5, 6};


class FakeViEWrapper : public cricket::ViEWrapper {
 public:
  explicit FakeViEWrapper(cricket::FakeWebRtcVideoEngine* engine)
      : cricket::ViEWrapper(engine,  // base
                            engine,  // codec
                            engine,  // capture
                            engine,  // network
                            engine,  // render
                            engine,  // rtp
                            engine,  // image
                            engine) {  // external decoder
  }
};

// Test fixture to test WebRtcVideoEngine with a fake webrtc::VideoEngine.
// Useful for testing failure paths.
class WebRtcVideoEngineTestFake : public testing::Test,
  public sigslot::has_slots<> {
 public:
  WebRtcVideoEngineTestFake()
      : vie_(kVideoCodecs, ARRAY_SIZE(kVideoCodecs)),
        cpu_monitor_(new talk_base::FakeCpuMonitor(
            talk_base::Thread::Current())),
        engine_(NULL,  // cricket::WebRtcVoiceEngine
                new FakeViEWrapper(&vie_), cpu_monitor_),
        channel_(NULL),
        voice_channel_(NULL),
        last_error_(cricket::VideoMediaChannel::ERROR_NONE) {
  }
  bool SetupEngine() {
    bool result = engine_.Init(talk_base::Thread::Current());
    if (result) {
      channel_ = engine_.CreateChannel(voice_channel_);
      channel_->SignalMediaError.connect(this,
          &WebRtcVideoEngineTestFake::OnMediaError);
      result = (channel_ != NULL);
    }
    return result;
  }
  void OnMediaError(uint32 ssrc, cricket::VideoMediaChannel::Error error) {
    last_error_ = error;
  }
  bool SendI420Frame(int width, int height) {
    if (NULL == channel_) {
      return false;
    }
    cricket::WebRtcVideoFrame frame;
    if (!frame.InitToBlack(width, height, 1, 1, 0, 0)) {
      return false;
    }
    cricket::FakeVideoCapturer capturer;
    channel_->SendFrame(&capturer, &frame);
    return true;
  }
  bool SendI420ScreencastFrame(int width, int height) {
    return SendI420ScreencastFrameWithTimestamp(width, height, 0);
  }
  bool SendI420ScreencastFrameWithTimestamp(
      int width, int height, int64 timestamp) {
    if (NULL == channel_) {
      return false;
    }
    cricket::WebRtcVideoFrame frame;
    if (!frame.InitToBlack(width, height, 1, 1, 0, 0)) {
      return false;
    }
    cricket::FakeVideoCapturer capturer;
    capturer.SetScreencast(true);
    channel_->SendFrame(&capturer, &frame);
    return true;
  }
  void VerifyCodecFeedbackParams(const cricket::VideoCodec& codec) {
    EXPECT_TRUE(codec.HasFeedbackParam(
        cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                               cricket::kParamValueEmpty)));
    EXPECT_TRUE(codec.HasFeedbackParam(
        cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                               cricket::kRtcpFbNackParamPli)));
    EXPECT_TRUE(codec.HasFeedbackParam(
        cricket::FeedbackParam(cricket::kRtcpFbParamRemb,
                               cricket::kParamValueEmpty)));
    EXPECT_TRUE(codec.HasFeedbackParam(
        cricket::FeedbackParam(cricket::kRtcpFbParamCcm,
                               cricket::kRtcpFbCcmParamFir)));
  }
  void VerifyVP8SendCodec(int channel_num,
                          unsigned int width,
                          unsigned int height,
                          unsigned int layers = 0,
                          unsigned int max_bitrate = kMaxBandwidthKbps,
                          unsigned int min_bitrate = kMinBandwidthKbps,
                          unsigned int start_bitrate = kStartBandwidthKbps,
                          unsigned int fps = 30,
                          unsigned int max_quantization = 0
                          ) {
    webrtc::VideoCodec gcodec;
    EXPECT_EQ(0, vie_.GetSendCodec(channel_num, gcodec));

    // Video codec properties.
    EXPECT_EQ(webrtc::kVideoCodecVP8, gcodec.codecType);
    EXPECT_STREQ("VP8", gcodec.plName);
    EXPECT_EQ(100, gcodec.plType);
    EXPECT_EQ(width, gcodec.width);
    EXPECT_EQ(height, gcodec.height);
    EXPECT_EQ(talk_base::_min(start_bitrate, max_bitrate), gcodec.startBitrate);
    EXPECT_EQ(max_bitrate, gcodec.maxBitrate);
    EXPECT_EQ(min_bitrate, gcodec.minBitrate);
    EXPECT_EQ(fps, gcodec.maxFramerate);
    // VP8 specific.
    EXPECT_FALSE(gcodec.codecSpecific.VP8.pictureLossIndicationOn);
    EXPECT_FALSE(gcodec.codecSpecific.VP8.feedbackModeOn);
    EXPECT_EQ(webrtc::kComplexityNormal, gcodec.codecSpecific.VP8.complexity);
    EXPECT_EQ(webrtc::kResilienceOff, gcodec.codecSpecific.VP8.resilience);
    EXPECT_EQ(max_quantization, gcodec.qpMax);
  }
  virtual void TearDown() {
    delete channel_;
    engine_.Terminate();
  }

 protected:
  cricket::FakeWebRtcVideoEngine vie_;
  cricket::FakeWebRtcVideoDecoderFactory decoder_factory_;
  cricket::FakeWebRtcVideoEncoderFactory encoder_factory_;
  talk_base::FakeCpuMonitor* cpu_monitor_;
  cricket::WebRtcVideoEngine engine_;
  cricket::WebRtcVideoMediaChannel* channel_;
  cricket::WebRtcVoiceMediaChannel* voice_channel_;
  cricket::VideoMediaChannel::Error last_error_;
};

// Test fixtures to test WebRtcVideoEngine with a real webrtc::VideoEngine.
class WebRtcVideoEngineTest
    : public VideoEngineTest<cricket::WebRtcVideoEngine> {
 protected:
  typedef VideoEngineTest<cricket::WebRtcVideoEngine> Base;
};
class WebRtcVideoMediaChannelTest
    : public VideoMediaChannelTest<
        cricket::WebRtcVideoEngine, cricket::WebRtcVideoMediaChannel> {
 protected:
  typedef VideoMediaChannelTest<cricket::WebRtcVideoEngine,
       cricket::WebRtcVideoMediaChannel> Base;
  virtual cricket::VideoCodec DefaultCodec() { return kVP8Codec; }
  virtual void SetUp() {
    Base::SetUp();
  }
  virtual void TearDown() {
    Base::TearDown();
  }
};

/////////////////////////
// Tests with fake ViE //
/////////////////////////

// Tests that our stub library "works".
TEST_F(WebRtcVideoEngineTestFake, StartupShutdown) {
  EXPECT_FALSE(vie_.IsInited());
  EXPECT_TRUE(engine_.Init(talk_base::Thread::Current()));
  EXPECT_TRUE(vie_.IsInited());
  engine_.Terminate();
}

// Tests that webrtc logs are logged when they should be.
TEST_F(WebRtcVideoEngineTest, WebRtcShouldLog) {
  const char webrtc_log[] = "WebRtcVideoEngineTest.WebRtcShouldLog";
  EXPECT_TRUE(engine_.Init(talk_base::Thread::Current()));
  engine_.SetLogging(talk_base::LS_INFO, "");
  std::string str;
  talk_base::StringStream stream(str);
  talk_base::LogMessage::AddLogToStream(&stream, talk_base::LS_INFO);
  EXPECT_EQ(talk_base::LS_INFO, talk_base::LogMessage::GetLogToStream(&stream));
  webrtc::Trace::Add(webrtc::kTraceStateInfo, webrtc::kTraceUndefined, 0,
                     webrtc_log);
  talk_base::Thread::Current()->ProcessMessages(100);
  talk_base::LogMessage::RemoveLogToStream(&stream);
  // Access |str| after LogMessage is done with it to avoid data racing.
  EXPECT_NE(std::string::npos, str.find(webrtc_log));
}

// Tests that webrtc logs are not logged when they should't be.
TEST_F(WebRtcVideoEngineTest, WebRtcShouldNotLog) {
  const char webrtc_log[] = "WebRtcVideoEngineTest.WebRtcShouldNotLog";
  EXPECT_TRUE(engine_.Init(talk_base::Thread::Current()));
  // WebRTC should never be logged lower than LS_INFO.
  engine_.SetLogging(talk_base::LS_WARNING, "");
  std::string str;
  talk_base::StringStream stream(str);
  // Make sure that WebRTC is not logged, even at lowest severity
  talk_base::LogMessage::AddLogToStream(&stream, talk_base::LS_SENSITIVE);
  EXPECT_EQ(talk_base::LS_SENSITIVE,
            talk_base::LogMessage::GetLogToStream(&stream));
  webrtc::Trace::Add(webrtc::kTraceStateInfo, webrtc::kTraceUndefined, 0,
                     webrtc_log);
  talk_base::Thread::Current()->ProcessMessages(10);
  EXPECT_EQ(std::string::npos, str.find(webrtc_log));
  talk_base::LogMessage::RemoveLogToStream(&stream);
}

// Tests that we can create and destroy a channel.
TEST_F(WebRtcVideoEngineTestFake, CreateChannel) {
  EXPECT_TRUE(engine_.Init(talk_base::Thread::Current()));
  channel_ = engine_.CreateChannel(voice_channel_);
  EXPECT_TRUE(channel_ != NULL);
  EXPECT_EQ(1, engine_.GetNumOfChannels());
  delete channel_;
  channel_ = NULL;
  EXPECT_EQ(0, engine_.GetNumOfChannels());
}

// Tests that we properly handle failures in CreateChannel.
TEST_F(WebRtcVideoEngineTestFake, CreateChannelFail) {
  vie_.set_fail_create_channel(true);
  EXPECT_TRUE(engine_.Init(talk_base::Thread::Current()));
  channel_ = engine_.CreateChannel(voice_channel_);
  EXPECT_TRUE(channel_ == NULL);
}

// Tests that we properly handle failures in AllocateExternalCaptureDevice.
TEST_F(WebRtcVideoEngineTestFake, AllocateExternalCaptureDeviceFail) {
  vie_.set_fail_alloc_capturer(true);
  EXPECT_TRUE(engine_.Init(talk_base::Thread::Current()));
  channel_ = engine_.CreateChannel(voice_channel_);
  EXPECT_TRUE(channel_ == NULL);
}

// Test that we apply our default codecs properly.
TEST_F(WebRtcVideoEngineTestFake, SetSendCodecs) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height);
  EXPECT_TRUE(vie_.GetHybridNackFecStatus(channel_num));
  EXPECT_FALSE(vie_.GetNackStatus(channel_num));
  // TODO(juberti): Check RTCP, PLI, TMMBR.
}

TEST_F(WebRtcVideoEngineTestFake, SetSendCodecsWithMinMaxBitrate) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  codecs[0].params[cricket::kCodecParamMinBitrate] = "10";
  codecs[0].params[cricket::kCodecParamMaxBitrate] = "20";
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  VerifyVP8SendCodec(
      channel_num, kVP8Codec.width, kVP8Codec.height, 0, 20, 10, 20);

  cricket::VideoCodec codec;
  EXPECT_TRUE(channel_->GetSendCodec(&codec));
  EXPECT_EQ("10", codec.params[cricket::kCodecParamMinBitrate]);
  EXPECT_EQ("20", codec.params[cricket::kCodecParamMaxBitrate]);
}

TEST_F(WebRtcVideoEngineTestFake, SetSendCodecsWithMinMaxBitrateInvalid) {
  EXPECT_TRUE(SetupEngine());
  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  codecs[0].params[cricket::kCodecParamMinBitrate] = "30";
  codecs[0].params[cricket::kCodecParamMaxBitrate] = "20";
  EXPECT_FALSE(channel_->SetSendCodecs(codecs));
}

TEST_F(WebRtcVideoEngineTestFake, SetSendCodecsWithLargeMinMaxBitrate) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  codecs[0].params[cricket::kCodecParamMinBitrate] = "1000";
  codecs[0].params[cricket::kCodecParamMaxBitrate] = "2000";
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  VerifyVP8SendCodec(
      channel_num, kVP8Codec.width, kVP8Codec.height, 0, 2000, 1000,
      1000);
}

TEST_F(WebRtcVideoEngineTestFake, SetSendCodecsWithMaxQuantization) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  codecs[0].params[cricket::kCodecParamMaxQuantization] = "21";
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  VerifyVP8SendCodec(
      channel_num, kVP8Codec.width, kVP8Codec.height, 0, 2000, 50, 300,
      30, 21);

  cricket::VideoCodec codec;
  EXPECT_TRUE(channel_->GetSendCodec(&codec));
  EXPECT_EQ("21", codec.params[cricket::kCodecParamMaxQuantization]);
}

TEST_F(WebRtcVideoEngineTestFake, SetOptionsWithMaxBitrate) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  codecs[0].params[cricket::kCodecParamMinBitrate] = "10";
  codecs[0].params[cricket::kCodecParamMaxBitrate] = "20";
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  VerifyVP8SendCodec(
      channel_num, kVP8Codec.width, kVP8Codec.height, 0, 20, 10, 20);

  // Verify that max bitrate doesn't change after SetOptions().
  cricket::VideoOptions options;
  options.video_noise_reduction.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  VerifyVP8SendCodec(
      channel_num, kVP8Codec.width, kVP8Codec.height, 0, 20, 10, 20);

  options.video_noise_reduction.Set(false);
  options.conference_mode.Set(false);
  EXPECT_TRUE(channel_->SetOptions(options));
  VerifyVP8SendCodec(
      channel_num, kVP8Codec.width, kVP8Codec.height, 0, 20, 10, 20);
}

TEST_F(WebRtcVideoEngineTestFake, SetOptionsWithLoweredBitrate) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  codecs[0].params[cricket::kCodecParamMinBitrate] = "50";
  codecs[0].params[cricket::kCodecParamMaxBitrate] = "100";
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  VerifyVP8SendCodec(
      channel_num, kVP8Codec.width, kVP8Codec.height, 0, 100, 50, 100);

  // Verify that min bitrate changes after SetOptions().
  cricket::VideoOptions options;
  options.lower_min_bitrate.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  VerifyVP8SendCodec(
      channel_num, kVP8Codec.width, kVP8Codec.height, 0, 100, 30, 100);
}

TEST_F(WebRtcVideoEngineTestFake, MaxBitrateResetWithConferenceMode) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  codecs[0].params[cricket::kCodecParamMinBitrate] = "10";
  codecs[0].params[cricket::kCodecParamMaxBitrate] = "20";
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  VerifyVP8SendCodec(
      channel_num, kVP8Codec.width, kVP8Codec.height, 0, 20, 10, 20);

  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  options.conference_mode.Set(false);
  EXPECT_TRUE(channel_->SetOptions(options));
  VerifyVP8SendCodec(
      channel_num, kVP8Codec.width, kVP8Codec.height, 0,
      kMaxBandwidthKbps, 10, 20);
}

// Verify the current send bitrate is used as start bitrate when reconfiguring
// the send codec.
TEST_F(WebRtcVideoEngineTestFake, StartSendBitrate) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(1)));
  int send_channel = vie_.GetLastChannel();
  cricket::VideoCodec codec(kVP8Codec);
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  const unsigned int kVideoMaxSendBitrateKbps = 2000;
  const unsigned int kVideoMinSendBitrateKbps = 50;
  const unsigned int kVideoDefaultStartSendBitrateKbps = 300;
  VerifyVP8SendCodec(send_channel, kVP8Codec.width, kVP8Codec.height, 0,
                     kVideoMaxSendBitrateKbps, kVideoMinSendBitrateKbps,
                     kVideoDefaultStartSendBitrateKbps);
  EXPECT_EQ(0, vie_.StartSend(send_channel));

  // Increase the send bitrate and verify it is used as start bitrate.
  const unsigned int kVideoSendBitrateBps = 768000;
  vie_.SetSendBitrates(send_channel, kVideoSendBitrateBps, 0, 0);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  VerifyVP8SendCodec(send_channel, kVP8Codec.width, kVP8Codec.height, 0,
                     kVideoMaxSendBitrateKbps, kVideoMinSendBitrateKbps,
                     kVideoSendBitrateBps / 1000);

  // Never set a start bitrate higher than the max bitrate.
  vie_.SetSendBitrates(send_channel, kVideoMaxSendBitrateKbps + 500, 0, 0);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  VerifyVP8SendCodec(send_channel, kVP8Codec.width, kVP8Codec.height, 0,
                     kVideoMaxSendBitrateKbps, kVideoMinSendBitrateKbps,
                     kVideoDefaultStartSendBitrateKbps);

  // Use the default start bitrate if the send bitrate is lower.
  vie_.SetSendBitrates(send_channel, kVideoDefaultStartSendBitrateKbps - 50, 0,
                       0);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  VerifyVP8SendCodec(send_channel, kVP8Codec.width, kVP8Codec.height, 0,
                     kVideoMaxSendBitrateKbps, kVideoMinSendBitrateKbps,
                     kVideoDefaultStartSendBitrateKbps);
}


// Test that we constrain send codecs properly.
TEST_F(WebRtcVideoEngineTestFake, ConstrainSendCodecs) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  // Set max settings of 640x400x30.
  EXPECT_TRUE(engine_.SetDefaultEncoderConfig(
    cricket::VideoEncoderConfig(kVP8Codec)));

  // Send codec format bigger than max setting.
  cricket::VideoCodec codec(kVP8Codec);
  codec.width = 1280;
  codec.height = 800;
  codec.framerate = 60;
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(codec);

  // Set send codec and verify codec has been constrained.
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height);
}

// Test that SetSendCodecs rejects bad format.
TEST_F(WebRtcVideoEngineTestFake, SetSendCodecsRejectBadFormat) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  // Set w = 0.
  cricket::VideoCodec codec(kVP8Codec);
  codec.width = 0;
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(codec);

  // Verify SetSendCodecs failed and send codec is not changed on engine.
  EXPECT_FALSE(channel_->SetSendCodecs(codec_list));
  webrtc::VideoCodec gcodec;
  // Set plType to something other than the value to test against ensuring
  // that failure will happen if it is not changed.
  gcodec.plType = 1;
  EXPECT_EQ(0, vie_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(0, gcodec.plType);

  // Set h = 0.
  codec_list[0].width = 640;
  codec_list[0].height = 0;

  // Verify SetSendCodecs failed and send codec is not changed on engine.
  EXPECT_FALSE(channel_->SetSendCodecs(codec_list));
  // Set plType to something other than the value to test against ensuring
  // that failure will happen if it is not changed.
  gcodec.plType = 1;
  EXPECT_EQ(0, vie_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(0, gcodec.plType);
}

// Test that SetSendCodecs rejects bad codec.
TEST_F(WebRtcVideoEngineTestFake, SetSendCodecsRejectBadCodec) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  // Set bad codec name.
  cricket::VideoCodec codec(kVP8Codec);
  codec.name = "bad";
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(codec);

  // Verify SetSendCodecs failed and send codec is not changed on engine.
  EXPECT_FALSE(channel_->SetSendCodecs(codec_list));
  webrtc::VideoCodec gcodec;
  // Set plType to something other than the value to test against ensuring
  // that failure will happen if it is not changed.
  gcodec.plType = 1;
  EXPECT_EQ(0, vie_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(0, gcodec.plType);
}

// Test that vie send codec is reset on new video frame size.
TEST_F(WebRtcVideoEngineTestFake, ResetVieSendCodecOnNewFrameSize) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  // Set send codec.
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(kVP8Codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(123)));
  EXPECT_TRUE(channel_->SetSend(true));

  // Capture a smaller frame and verify vie send codec has been reset to
  // the new size.
  SendI420Frame(kVP8Codec.width / 2, kVP8Codec.height / 2);
  VerifyVP8SendCodec(channel_num, kVP8Codec.width / 2, kVP8Codec.height / 2);

  // Capture a frame bigger than send_codec_ and verify vie send codec has been
  // reset (and clipped) to send_codec_.
  SendI420Frame(kVP8Codec.width * 2, kVP8Codec.height * 2);
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height);
}

// Test that we set our inbound codecs properly.
TEST_F(WebRtcVideoEngineTestFake, SetRecvCodecs) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVP8Codec);
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));

  webrtc::VideoCodec wcodec;
  EXPECT_TRUE(engine_.ConvertFromCricketVideoCodec(kVP8Codec, &wcodec));
  EXPECT_TRUE(vie_.ReceiveCodecRegistered(channel_num, wcodec));
}

// Test that channel connects and disconnects external capturer correctly.
TEST_F(WebRtcVideoEngineTestFake, HasExternalCapturer) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  EXPECT_EQ(1, vie_.GetNumCapturers());
  int capture_id = vie_.GetCaptureId(channel_num);
  EXPECT_EQ(channel_num, vie_.GetCaptureChannelId(capture_id));

  // Delete the channel should disconnect the capturer.
  delete channel_;
  channel_ = NULL;
  EXPECT_EQ(0, vie_.GetNumCapturers());
}

// Test that channel adds and removes renderer correctly.
TEST_F(WebRtcVideoEngineTestFake, HasRenderer) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  EXPECT_TRUE(vie_.GetHasRenderer(channel_num));
  EXPECT_FALSE(vie_.GetRenderStarted(channel_num));
}

// Test that rtcp is enabled on the channel.
TEST_F(WebRtcVideoEngineTestFake, RtcpEnabled) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  EXPECT_EQ(webrtc::kRtcpCompound_RFC4585, vie_.GetRtcpStatus(channel_num));
}

// Test that key frame request method is set on the channel.
TEST_F(WebRtcVideoEngineTestFake, KeyFrameRequestEnabled) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  EXPECT_EQ(webrtc::kViEKeyFrameRequestPliRtcp,
            vie_.GetKeyFrameRequestMethod(channel_num));
}

// Test that remb receive and send is enabled for the default channel in a 1:1
// call.
TEST_F(WebRtcVideoEngineTestFake, RembEnabled) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(1)));
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));
  EXPECT_TRUE(vie_.GetRembStatusBwPartition(channel_num));
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_TRUE(vie_.GetRembStatusBwPartition(channel_num));
  EXPECT_TRUE(vie_.GetRembStatusContribute(channel_num));
}

// When in conference mode, test that remb is enabled on a receive channel but
// not for the default channel and that it uses the default channel for sending
// remb packets.
TEST_F(WebRtcVideoEngineTestFake, RembEnabledOnReceiveChannels) {
  EXPECT_TRUE(SetupEngine());
  int default_channel = vie_.GetLastChannel();
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(1)));
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));
  EXPECT_TRUE(vie_.GetRembStatusBwPartition(default_channel));
  EXPECT_TRUE(vie_.GetRembStatusContribute(default_channel));
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  int new_channel_num = vie_.GetLastChannel();
  EXPECT_NE(default_channel, new_channel_num);

  EXPECT_TRUE(vie_.GetRembStatusBwPartition(default_channel));
  EXPECT_TRUE(vie_.GetRembStatusContribute(default_channel));
  EXPECT_FALSE(vie_.GetRembStatusBwPartition(new_channel_num));
  EXPECT_TRUE(vie_.GetRembStatusContribute(new_channel_num));
}

TEST_F(WebRtcVideoEngineTestFake, RecvStreamWithRtx) {
  EXPECT_TRUE(SetupEngine());
  int default_channel = vie_.GetLastChannel();
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimWithRtxStreamParams("cname",
                                            MAKE_VECTOR(kSsrcs3),
                                            MAKE_VECTOR(kRtxSsrcs3))));
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::CreateSimWithRtxStreamParams("cname",
                                            MAKE_VECTOR(kSsrcs1),
                                            MAKE_VECTOR(kRtxSsrc1))));
  int new_channel_num = vie_.GetLastChannel();
  EXPECT_NE(default_channel, new_channel_num);
  EXPECT_EQ(4, vie_.GetRemoteRtxSsrc(new_channel_num));
}

TEST_F(WebRtcVideoEngineTestFake, RecvStreamNoRtx) {
  EXPECT_TRUE(SetupEngine());
  int default_channel = vie_.GetLastChannel();
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimWithRtxStreamParams("cname",
                                            MAKE_VECTOR(kSsrcs3),
                                            MAKE_VECTOR(kRtxSsrcs3))));
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  int new_channel_num = vie_.GetLastChannel();
  EXPECT_NE(default_channel, new_channel_num);
  EXPECT_EQ(-1, vie_.GetRemoteRtxSsrc(new_channel_num));
}

// Test support for RTP timestamp offset header extension.
TEST_F(WebRtcVideoEngineTestFake, RtpTimestampOffsetHeaderExtensions) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));

  // Verify extensions are off by default.
  EXPECT_EQ(0, vie_.GetSendRtpTimestampOffsetExtensionId(channel_num));
  EXPECT_EQ(0, vie_.GetReceiveRtpTimestampOffsetExtensionId(channel_num));

  // Enable RTP timestamp extension.
  const int id = 14;
  std::vector<cricket::RtpHeaderExtension> extensions;
  extensions.push_back(cricket::RtpHeaderExtension(
      "urn:ietf:params:rtp-hdrext:toffset", id));

  // Verify the send extension id.
  EXPECT_TRUE(channel_->SetSendRtpHeaderExtensions(extensions));
  EXPECT_EQ(id, vie_.GetSendRtpTimestampOffsetExtensionId(channel_num));

  // Remove the extension id.
  std::vector<cricket::RtpHeaderExtension> empty_extensions;
  EXPECT_TRUE(channel_->SetSendRtpHeaderExtensions(empty_extensions));
  EXPECT_EQ(0, vie_.GetSendRtpTimestampOffsetExtensionId(channel_num));

  // Verify receive extension id.
  EXPECT_TRUE(channel_->SetRecvRtpHeaderExtensions(extensions));
  EXPECT_EQ(id, vie_.GetReceiveRtpTimestampOffsetExtensionId(channel_num));

  // Add a new receive stream and verify the extension is set.
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  int new_channel_num = vie_.GetLastChannel();
  EXPECT_NE(channel_num, new_channel_num);
  EXPECT_EQ(id, vie_.GetReceiveRtpTimestampOffsetExtensionId(new_channel_num));

  // Remove the extension id.
  EXPECT_TRUE(channel_->SetRecvRtpHeaderExtensions(empty_extensions));
  EXPECT_EQ(0, vie_.GetReceiveRtpTimestampOffsetExtensionId(channel_num));
  EXPECT_EQ(0, vie_.GetReceiveRtpTimestampOffsetExtensionId(new_channel_num));
}

// Test support for absolute send time header extension.
TEST_F(WebRtcVideoEngineTestFake, AbsoluteSendTimeHeaderExtensions) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));

  // Verify extensions are off by default.
  EXPECT_EQ(0, vie_.GetSendAbsoluteSendTimeExtensionId(channel_num));
  EXPECT_EQ(0, vie_.GetReceiveAbsoluteSendTimeExtensionId(channel_num));

  // Enable RTP timestamp extension.
  const int id = 12;
  std::vector<cricket::RtpHeaderExtension> extensions;
  extensions.push_back(cricket::RtpHeaderExtension(
      "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time", id));

  // Verify the send extension id.
  EXPECT_TRUE(channel_->SetSendRtpHeaderExtensions(extensions));
  EXPECT_EQ(id, vie_.GetSendAbsoluteSendTimeExtensionId(channel_num));

  // Remove the extension id.
  std::vector<cricket::RtpHeaderExtension> empty_extensions;
  EXPECT_TRUE(channel_->SetSendRtpHeaderExtensions(empty_extensions));
  EXPECT_EQ(0, vie_.GetSendAbsoluteSendTimeExtensionId(channel_num));

  // Verify receive extension id.
  EXPECT_TRUE(channel_->SetRecvRtpHeaderExtensions(extensions));
  EXPECT_EQ(id, vie_.GetReceiveAbsoluteSendTimeExtensionId(channel_num));

  // Add a new receive stream and verify the extension is set.
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  int new_channel_num = vie_.GetLastChannel();
  EXPECT_NE(channel_num, new_channel_num);
  EXPECT_EQ(id, vie_.GetReceiveAbsoluteSendTimeExtensionId(new_channel_num));

  // Remove the extension id.
  EXPECT_TRUE(channel_->SetRecvRtpHeaderExtensions(empty_extensions));
  EXPECT_EQ(0, vie_.GetReceiveAbsoluteSendTimeExtensionId(channel_num));
  EXPECT_EQ(0, vie_.GetReceiveAbsoluteSendTimeExtensionId(new_channel_num));
}

TEST_F(WebRtcVideoEngineTestFake, LeakyBucketTest) {
  EXPECT_TRUE(SetupEngine());

  // Verify this is off by default.
  EXPECT_TRUE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(1)));
  int first_send_channel = vie_.GetLastChannel();
  EXPECT_FALSE(vie_.GetTransmissionSmoothingStatus(first_send_channel));

  // Enable the experiment and verify.
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  options.video_leaky_bucket.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  EXPECT_TRUE(vie_.GetTransmissionSmoothingStatus(first_send_channel));

  // Add a receive channel and verify leaky bucket isn't enabled.
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  int recv_channel_num = vie_.GetLastChannel();
  EXPECT_NE(first_send_channel, recv_channel_num);
  EXPECT_FALSE(vie_.GetTransmissionSmoothingStatus(recv_channel_num));

  // Add a new send stream and verify leaky bucket is enabled from start.
  EXPECT_TRUE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(3)));
  int second_send_channel = vie_.GetLastChannel();
  EXPECT_NE(first_send_channel, second_send_channel);
  EXPECT_TRUE(vie_.GetTransmissionSmoothingStatus(second_send_channel));
}

TEST_F(WebRtcVideoEngineTestFake, BufferedModeLatency) {
  EXPECT_TRUE(SetupEngine());

  // Verify this is off by default.
  EXPECT_TRUE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(1)));
  int first_send_channel = vie_.GetLastChannel();
  EXPECT_EQ(0, vie_.GetSenderTargetDelay(first_send_channel));
  EXPECT_EQ(0, vie_.GetReceiverTargetDelay(first_send_channel));

  // Enable the experiment and verify. The default channel will have both
  // sender and receiver buffered mode enabled.
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  options.buffered_mode_latency.Set(100);
  EXPECT_TRUE(channel_->SetOptions(options));
  EXPECT_EQ(100, vie_.GetSenderTargetDelay(first_send_channel));
  EXPECT_EQ(100, vie_.GetReceiverTargetDelay(first_send_channel));

  // Add a receive channel and verify sender buffered mode isn't enabled.
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  int recv_channel_num = vie_.GetLastChannel();
  EXPECT_NE(first_send_channel, recv_channel_num);
  EXPECT_EQ(0, vie_.GetSenderTargetDelay(recv_channel_num));
  EXPECT_EQ(100, vie_.GetReceiverTargetDelay(recv_channel_num));

  // Add a new send stream and verify sender buffered mode is enabled.
  EXPECT_TRUE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(3)));
  int second_send_channel = vie_.GetLastChannel();
  EXPECT_NE(first_send_channel, second_send_channel);
  EXPECT_EQ(100, vie_.GetSenderTargetDelay(second_send_channel));
  EXPECT_EQ(0, vie_.GetReceiverTargetDelay(second_send_channel));

  // Disable sender buffered mode and verify.
  options.buffered_mode_latency.Set(cricket::kBufferedModeDisabled);
  EXPECT_TRUE(channel_->SetOptions(options));
  EXPECT_EQ(0, vie_.GetSenderTargetDelay(first_send_channel));
  EXPECT_EQ(0, vie_.GetReceiverTargetDelay(first_send_channel));
  EXPECT_EQ(0, vie_.GetSenderTargetDelay(second_send_channel));
  EXPECT_EQ(0, vie_.GetReceiverTargetDelay(second_send_channel));
  EXPECT_EQ(0, vie_.GetSenderTargetDelay(recv_channel_num));
  EXPECT_EQ(0, vie_.GetReceiverTargetDelay(recv_channel_num));
}

TEST_F(WebRtcVideoEngineTestFake, AdditiveVideoOptions) {
  EXPECT_TRUE(SetupEngine());

  EXPECT_TRUE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(1)));
  int first_send_channel = vie_.GetLastChannel();
  EXPECT_EQ(0, vie_.GetSenderTargetDelay(first_send_channel));
  EXPECT_EQ(0, vie_.GetReceiverTargetDelay(first_send_channel));

  cricket::VideoOptions options1;
  options1.buffered_mode_latency.Set(100);
  EXPECT_TRUE(channel_->SetOptions(options1));
  EXPECT_EQ(100, vie_.GetSenderTargetDelay(first_send_channel));
  EXPECT_EQ(100, vie_.GetReceiverTargetDelay(first_send_channel));
  EXPECT_FALSE(vie_.GetTransmissionSmoothingStatus(first_send_channel));

  cricket::VideoOptions options2;
  options2.video_leaky_bucket.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options2));
  EXPECT_TRUE(vie_.GetTransmissionSmoothingStatus(first_send_channel));
  // The buffered_mode_latency still takes effect.
  EXPECT_EQ(100, vie_.GetSenderTargetDelay(first_send_channel));
  EXPECT_EQ(100, vie_.GetReceiverTargetDelay(first_send_channel));

  options1.buffered_mode_latency.Set(50);
  EXPECT_TRUE(channel_->SetOptions(options1));
  EXPECT_EQ(50, vie_.GetSenderTargetDelay(first_send_channel));
  EXPECT_EQ(50, vie_.GetReceiverTargetDelay(first_send_channel));
  // The video_leaky_bucket still takes effect.
  EXPECT_TRUE(vie_.GetTransmissionSmoothingStatus(first_send_channel));
}

// Test that AddRecvStream doesn't create new channel for 1:1 call.
TEST_F(WebRtcVideoEngineTestFake, AddRecvStream1On1) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  EXPECT_EQ(channel_num, vie_.GetLastChannel());
}

// Test that NACK, PLI and REMB are enabled for internal codec.
TEST_F(WebRtcVideoEngineTestFake, InternalCodecFeedbackParams) {
  EXPECT_TRUE(SetupEngine());

  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  // Vp8 will appear at the beginning.
  size_t pos = 0;
  EXPECT_EQ("VP8", codecs[pos].name);
  VerifyCodecFeedbackParams(codecs[pos]);
}

// Test that AddRecvStream doesn't change remb for 1:1 call.
TEST_F(WebRtcVideoEngineTestFake, NoRembChangeAfterAddRecvStream) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(1)));
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));
  EXPECT_TRUE(vie_.GetRembStatusBwPartition(channel_num));
  EXPECT_TRUE(vie_.GetRembStatusContribute(channel_num));
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  EXPECT_TRUE(vie_.GetRembStatusBwPartition(channel_num));
  EXPECT_TRUE(vie_.GetRembStatusContribute(channel_num));
}

// Verify default REMB setting and that it can be turned on and off.
TEST_F(WebRtcVideoEngineTestFake, RembOnOff) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  // Verify REMB sending is always off by default.
  EXPECT_FALSE(vie_.GetRembStatusBwPartition(channel_num));

  // Verify that REMB is turned on when setting default codecs since the
  // default codecs have REMB enabled.
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));
  EXPECT_TRUE(vie_.GetRembStatusBwPartition(channel_num));

  // Verify that REMB is turned off when codecs without REMB are set.
  std::vector<cricket::VideoCodec> codecs = engine_.codecs();
  // Clearing the codecs' FeedbackParams and setting send codecs should disable
  // REMB.
  for (std::vector<cricket::VideoCodec>::iterator iter = codecs.begin();
       iter != codecs.end(); ++iter) {
    // Intersecting with empty will clear the FeedbackParams.
    cricket::FeedbackParams empty_params;
    iter->feedback_params.Intersect(empty_params);
    EXPECT_TRUE(iter->feedback_params.params().empty());
  }
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_FALSE(vie_.GetRembStatusBwPartition(channel_num));
}

// Test that nack is enabled on the channel if we don't offer red/fec.
TEST_F(WebRtcVideoEngineTestFake, NackEnabled) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  codecs.resize(1);  // toss out red and ulpfec
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_TRUE(vie_.GetNackStatus(channel_num));
}

// Test that we enable hybrid NACK FEC mode.
TEST_F(WebRtcVideoEngineTestFake, HybridNackFec) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  EXPECT_TRUE(channel_->SetRecvCodecs(engine_.codecs()));
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));
  EXPECT_TRUE(vie_.GetHybridNackFecStatus(channel_num));
  EXPECT_FALSE(vie_.GetNackStatus(channel_num));
}

// Test that we enable hybrid NACK FEC mode when calling SetSendCodecs and
// SetReceiveCodecs in reversed order.
TEST_F(WebRtcVideoEngineTestFake, HybridNackFecReversedOrder) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));
  EXPECT_TRUE(channel_->SetRecvCodecs(engine_.codecs()));
  EXPECT_TRUE(vie_.GetHybridNackFecStatus(channel_num));
  EXPECT_FALSE(vie_.GetNackStatus(channel_num));
}

// Test NACK vs Hybrid NACK/FEC interop call setup, i.e. only use NACK even if
// red/fec is offered as receive codec.
TEST_F(WebRtcVideoEngineTestFake, VideoProtectionInterop) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  std::vector<cricket::VideoCodec> recv_codecs(engine_.codecs());
  std::vector<cricket::VideoCodec> send_codecs(engine_.codecs());
  // Only add VP8 as send codec.
  send_codecs.resize(1);
  EXPECT_TRUE(channel_->SetRecvCodecs(recv_codecs));
  EXPECT_TRUE(channel_->SetSendCodecs(send_codecs));
  EXPECT_FALSE(vie_.GetHybridNackFecStatus(channel_num));
  EXPECT_TRUE(vie_.GetNackStatus(channel_num));
}

// Test NACK vs Hybrid NACK/FEC interop call setup, i.e. only use NACK even if
// red/fec is offered as receive codec. Call order reversed compared to
// VideoProtectionInterop.
TEST_F(WebRtcVideoEngineTestFake, VideoProtectionInteropReversed) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  std::vector<cricket::VideoCodec> recv_codecs(engine_.codecs());
  std::vector<cricket::VideoCodec> send_codecs(engine_.codecs());
  // Only add VP8 as send codec.
  send_codecs.resize(1);
  EXPECT_TRUE(channel_->SetSendCodecs(send_codecs));
  EXPECT_TRUE(channel_->SetRecvCodecs(recv_codecs));
  EXPECT_FALSE(vie_.GetHybridNackFecStatus(channel_num));
  EXPECT_TRUE(vie_.GetNackStatus(channel_num));
}

// Test that NACK, not hybrid mode, is enabled in conference mode.
TEST_F(WebRtcVideoEngineTestFake, HybridNackFecConference) {
  EXPECT_TRUE(SetupEngine());
  // Setup the send channel.
  int send_channel_num = vie_.GetLastChannel();
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  EXPECT_TRUE(channel_->SetRecvCodecs(engine_.codecs()));
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));
  EXPECT_FALSE(vie_.GetHybridNackFecStatus(send_channel_num));
  EXPECT_TRUE(vie_.GetNackStatus(send_channel_num));
  // Add a receive stream.
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  int receive_channel_num = vie_.GetLastChannel();
  EXPECT_FALSE(vie_.GetHybridNackFecStatus(receive_channel_num));
  EXPECT_TRUE(vie_.GetNackStatus(receive_channel_num));
}

// Test that when AddRecvStream in conference mode, a new channel is created
// for receiving. And the new channel's "original channel" is the send channel.
TEST_F(WebRtcVideoEngineTestFake, AddRemoveRecvStreamConference) {
  EXPECT_TRUE(SetupEngine());
  // Setup the send channel.
  int send_channel_num = vie_.GetLastChannel();
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  // Add a receive stream.
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  int receive_channel_num = vie_.GetLastChannel();
  EXPECT_EQ(send_channel_num, vie_.GetOriginalChannelId(receive_channel_num));
  EXPECT_TRUE(channel_->RemoveRecvStream(1));
  EXPECT_FALSE(vie_.IsChannel(receive_channel_num));
}

// Test that we can create a channel and start/stop rendering out on it.
TEST_F(WebRtcVideoEngineTestFake, SetRender) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  // Verify we can start/stop/start/stop rendering.
  EXPECT_TRUE(channel_->SetRender(true));
  EXPECT_TRUE(vie_.GetRenderStarted(channel_num));
  EXPECT_TRUE(channel_->SetRender(false));
  EXPECT_FALSE(vie_.GetRenderStarted(channel_num));
  EXPECT_TRUE(channel_->SetRender(true));
  EXPECT_TRUE(vie_.GetRenderStarted(channel_num));
  EXPECT_TRUE(channel_->SetRender(false));
  EXPECT_FALSE(vie_.GetRenderStarted(channel_num));
}

// Test that we can create a channel and start/stop sending out on it.
TEST_F(WebRtcVideoEngineTestFake, SetSend) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  // Set send codecs on the channel.
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVP8Codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(123)));

  // Verify we can start/stop/start/stop sending.
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_TRUE(vie_.GetSend(channel_num));
  EXPECT_TRUE(channel_->SetSend(false));
  EXPECT_FALSE(vie_.GetSend(channel_num));
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_TRUE(vie_.GetSend(channel_num));
  EXPECT_TRUE(channel_->SetSend(false));
  EXPECT_FALSE(vie_.GetSend(channel_num));
}

// Test that we set bandwidth properly when using full auto bandwidth mode.
TEST_F(WebRtcVideoEngineTestFake, SetBandwidthAuto) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));
  EXPECT_TRUE(channel_->SetSendBandwidth(true, cricket::kAutoBandwidth));
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height);
}

// Test that we set bandwidth properly when using auto with upper bound.
TEST_F(WebRtcVideoEngineTestFake, SetBandwidthAutoCapped) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));
  EXPECT_TRUE(channel_->SetSendBandwidth(true, 768000));
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height, 0, 768U);
}

// Test that we set bandwidth properly when using a fixed bandwidth.
TEST_F(WebRtcVideoEngineTestFake, SetBandwidthFixed) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));
  EXPECT_TRUE(channel_->SetSendBandwidth(false, 768000));
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height, 0,
                     768U, 768U, 768U);
}

// Test that SetSendBandwidth is ignored in conference mode.
TEST_F(WebRtcVideoEngineTestFake, SetBandwidthInConference) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height);

  // Set send bandwidth.
  EXPECT_TRUE(channel_->SetSendBandwidth(false, 768000));

  // Verify bitrate not changed.
  webrtc::VideoCodec gcodec;
  EXPECT_EQ(0, vie_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(kMinBandwidthKbps, gcodec.minBitrate);
  EXPECT_EQ(kStartBandwidthKbps, gcodec.startBitrate);
  EXPECT_EQ(kMaxBandwidthKbps, gcodec.maxBitrate);
  EXPECT_NE(768U, gcodec.minBitrate);
  EXPECT_NE(768U, gcodec.startBitrate);
  EXPECT_NE(768U, gcodec.maxBitrate);
}

// Test that sending screencast frames doesn't change bitrate.
TEST_F(WebRtcVideoEngineTestFake, SetBandwidthScreencast) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  // Set send codec.
  cricket::VideoCodec codec(kVP8Codec);
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(codec);
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(123)));
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  EXPECT_TRUE(channel_->SetSendBandwidth(false, 111000));
  EXPECT_TRUE(channel_->SetSend(true));

  SendI420ScreencastFrame(kVP8Codec.width, kVP8Codec.height);
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height, 0,
                     111, 111, 111);
}


// Test SetSendSsrc.
TEST_F(WebRtcVideoEngineTestFake, SetSendSsrcAndCname) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  cricket::StreamParams stream;
  stream.ssrcs.push_back(1234);
  stream.cname = "cname";
  channel_->AddSendStream(stream);

  unsigned int ssrc = 0;
  EXPECT_EQ(0, vie_.GetLocalSSRC(channel_num, ssrc));
  EXPECT_EQ(1234U, ssrc);
  EXPECT_EQ(1, vie_.GetNumSsrcs(channel_num));

  char rtcp_cname[256];
  EXPECT_EQ(0, vie_.GetRTCPCName(channel_num, rtcp_cname));
  EXPECT_STREQ("cname", rtcp_cname);
}


// Test that the local SSRC is the same on sending and receiving channels if the
// receive channel is created before the send channel.
TEST_F(WebRtcVideoEngineTestFake, SetSendSsrcAfterCreatingReceiveChannel) {
  EXPECT_TRUE(SetupEngine());

  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  int receive_channel_num = vie_.GetLastChannel();
  cricket::StreamParams stream = cricket::StreamParams::CreateLegacy(1234);
  EXPECT_TRUE(channel_->AddSendStream(stream));
  int send_channel_num = vie_.GetLastChannel();
  unsigned int ssrc = 0;
  EXPECT_EQ(0, vie_.GetLocalSSRC(send_channel_num, ssrc));
  EXPECT_EQ(1234U, ssrc);
  EXPECT_EQ(1, vie_.GetNumSsrcs(send_channel_num));
  ssrc = 0;
  EXPECT_EQ(0, vie_.GetLocalSSRC(receive_channel_num, ssrc));
  EXPECT_EQ(1234U, ssrc);
  EXPECT_EQ(1, vie_.GetNumSsrcs(receive_channel_num));
}


// Test SetOptions with denoising flag.
TEST_F(WebRtcVideoEngineTestFake, SetOptionsWithDenoising) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_EQ(1, vie_.GetNumCapturers());
  int channel_num = vie_.GetLastChannel();
  int capture_id = vie_.GetCaptureId(channel_num);
  // Set send codecs on the channel.
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVP8Codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  // Set options with OPT_VIDEO_NOISE_REDUCTION flag.
  cricket::VideoOptions options;
  options.video_noise_reduction.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));

  // Verify capture has denoising turned on.
  webrtc::VideoCodec send_codec;
  memset(&send_codec, 0, sizeof(send_codec));  // avoid uninitialized warning
  EXPECT_EQ(0, vie_.GetSendCodec(channel_num, send_codec));
  EXPECT_TRUE(send_codec.codecSpecific.VP8.denoisingOn);
  EXPECT_FALSE(vie_.GetCaptureDenoising(capture_id));

  // Set options back to zero.
  options.video_noise_reduction.Set(false);
  EXPECT_TRUE(channel_->SetOptions(options));

  // Verify capture has denoising turned off.
  EXPECT_EQ(0, vie_.GetSendCodec(channel_num, send_codec));
  EXPECT_FALSE(send_codec.codecSpecific.VP8.denoisingOn);
  EXPECT_FALSE(vie_.GetCaptureDenoising(capture_id));
}

TEST_F(WebRtcVideoEngineTestFake, MultipleSendStreamsWithOneCapturer) {
  EXPECT_TRUE(SetupEngine());

  // Start the capturer
  cricket::FakeVideoCapturer capturer;
  cricket::VideoFormat capture_format_vga = cricket::VideoFormat(640, 480,
        cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420);
  EXPECT_EQ(cricket::CS_RUNNING, capturer.Start(capture_format_vga));

  // Add send streams and connect the capturer
  for (unsigned int i = 0; i < sizeof(kSsrcs2)/sizeof(kSsrcs2[0]); ++i) {
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kSsrcs2[i])));
    // Register the capturer to the ssrc.
    EXPECT_TRUE(channel_->SetCapturer(kSsrcs2[i], &capturer));
  }

  const int channel0 = vie_.GetChannelFromLocalSsrc(kSsrcs2[0]);
  ASSERT_NE(-1, channel0);
  const int channel1 = vie_.GetChannelFromLocalSsrc(kSsrcs2[1]);
  ASSERT_NE(-1, channel1);
  ASSERT_NE(channel0, channel1);

  // Set send codec.
  std::vector<cricket::VideoCodec> codecs;
  cricket::VideoCodec send_codec(100, "VP8", 640, 480, 30, 0);
  codecs.push_back(send_codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  EXPECT_TRUE(capturer.CaptureFrame());
  EXPECT_EQ(1, vie_.GetIncomingFrameNum(channel0));
  EXPECT_EQ(1, vie_.GetIncomingFrameNum(channel1));

  EXPECT_TRUE(channel_->RemoveSendStream(kSsrcs2[0]));
  EXPECT_TRUE(capturer.CaptureFrame());
  // channel0 is the default channel, so it won't be deleted.
  // But it should be disconnected from the capturer.
  EXPECT_EQ(1, vie_.GetIncomingFrameNum(channel0));
  EXPECT_EQ(2, vie_.GetIncomingFrameNum(channel1));

  EXPECT_TRUE(channel_->RemoveSendStream(kSsrcs2[1]));
  EXPECT_TRUE(capturer.CaptureFrame());
  EXPECT_EQ(1, vie_.GetIncomingFrameNum(channel0));
  // channel1 has already been deleted.
  EXPECT_EQ(-1, vie_.GetIncomingFrameNum(channel1));
}


// Disabled since its flaky: b/11288120
TEST_F(WebRtcVideoEngineTestFake, DISABLED_SendReceiveBitratesStats) {
  EXPECT_TRUE(SetupEngine());
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(1)));
  int send_channel = vie_.GetLastChannel();
  cricket::VideoCodec codec(kVP8Codec720p);
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));

  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::StreamParams::CreateLegacy(2)));
  int first_receive_channel = vie_.GetLastChannel();
  EXPECT_NE(send_channel, first_receive_channel);
  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::StreamParams::CreateLegacy(3)));
  int second_receive_channel = vie_.GetLastChannel();
  EXPECT_NE(first_receive_channel, second_receive_channel);

  cricket::VideoMediaInfo info;
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.bw_estimations.size());
  ASSERT_EQ(0, info.bw_estimations[0].actual_enc_bitrate);
  ASSERT_EQ(0, info.bw_estimations[0].transmit_bitrate);
  ASSERT_EQ(0, info.bw_estimations[0].retransmit_bitrate);
  ASSERT_EQ(0, info.bw_estimations[0].available_send_bandwidth);
  ASSERT_EQ(0, info.bw_estimations[0].available_recv_bandwidth);
  ASSERT_EQ(0, info.bw_estimations[0].target_enc_bitrate);

  // Start sending and receiving on one of the channels and verify bitrates.
  EXPECT_EQ(0, vie_.StartSend(send_channel));
  int send_video_bitrate = 800;
  int send_fec_bitrate = 100;
  int send_nack_bitrate = 20;
  int send_total_bitrate = send_video_bitrate + send_fec_bitrate +
      send_nack_bitrate;
  int send_bandwidth = 950;
  vie_.SetSendBitrates(send_channel, send_video_bitrate, send_fec_bitrate,
                       send_nack_bitrate);
  vie_.SetSendBandwidthEstimate(send_channel, send_bandwidth);

  EXPECT_EQ(0, vie_.StartReceive(first_receive_channel));
  int first_channel_receive_bandwidth = 600;
  vie_.SetReceiveBandwidthEstimate(first_receive_channel,
                                   first_channel_receive_bandwidth);

  info.Clear();
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.bw_estimations.size());
  ASSERT_EQ(send_video_bitrate, info.bw_estimations[0].actual_enc_bitrate);
  ASSERT_EQ(send_total_bitrate, info.bw_estimations[0].transmit_bitrate);
  ASSERT_EQ(send_nack_bitrate, info.bw_estimations[0].retransmit_bitrate);
  ASSERT_EQ(send_bandwidth, info.bw_estimations[0].available_send_bandwidth);
  ASSERT_EQ(first_channel_receive_bandwidth,
            info.bw_estimations[0].available_recv_bandwidth);
  ASSERT_EQ(send_video_bitrate, info.bw_estimations[0].target_enc_bitrate);

  // Start receiving on the second channel and verify received rate.
  EXPECT_EQ(0, vie_.StartReceive(second_receive_channel));
  int second_channel_receive_bandwidth = 100;
  vie_.SetReceiveBandwidthEstimate(second_receive_channel,
                                   second_channel_receive_bandwidth);

  info.Clear();
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.bw_estimations.size());
  ASSERT_EQ(send_video_bitrate, info.bw_estimations[0].actual_enc_bitrate);
  ASSERT_EQ(send_total_bitrate, info.bw_estimations[0].transmit_bitrate);
  ASSERT_EQ(send_nack_bitrate, info.bw_estimations[0].retransmit_bitrate);
  ASSERT_EQ(send_bandwidth, info.bw_estimations[0].available_send_bandwidth);
  ASSERT_EQ(first_channel_receive_bandwidth + second_channel_receive_bandwidth,
            info.bw_estimations[0].available_recv_bandwidth);
  ASSERT_EQ(send_video_bitrate, info.bw_estimations[0].target_enc_bitrate);
}

TEST_F(WebRtcVideoEngineTestFake, TestSetAdaptInputToCpuUsage) {
  EXPECT_TRUE(SetupEngine());
  cricket::VideoOptions options_in, options_out;
  bool cpu_adapt = false;
  channel_->SetOptions(options_in);
  EXPECT_TRUE(channel_->GetOptions(&options_out));
  EXPECT_FALSE(options_out.adapt_input_to_cpu_usage.Get(&cpu_adapt));
  // Set adapt input CPU usage option.
  options_in.adapt_input_to_cpu_usage.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options_in));
  EXPECT_TRUE(channel_->GetOptions(&options_out));
  EXPECT_TRUE(options_out.adapt_input_to_cpu_usage.Get(&cpu_adapt));
  EXPECT_TRUE(cpu_adapt);
}

TEST_F(WebRtcVideoEngineTestFake, TestSetCpuThreshold) {
  EXPECT_TRUE(SetupEngine());
  float low, high;
  cricket::VideoOptions options_in, options_out;
  // Verify that initial values are set.
  EXPECT_TRUE(channel_->GetOptions(&options_out));
  EXPECT_TRUE(options_out.system_low_adaptation_threshhold.Get(&low));
  EXPECT_EQ(low, 0.65f);
  EXPECT_TRUE(options_out.system_high_adaptation_threshhold.Get(&high));
  EXPECT_EQ(high, 0.85f);
  // Set new CPU threshold values.
  options_in.system_low_adaptation_threshhold.Set(0.45f);
  options_in.system_high_adaptation_threshhold.Set(0.95f);
  EXPECT_TRUE(channel_->SetOptions(options_in));
  EXPECT_TRUE(channel_->GetOptions(&options_out));
  EXPECT_TRUE(options_out.system_low_adaptation_threshhold.Get(&low));
  EXPECT_EQ(low, 0.45f);
  EXPECT_TRUE(options_out.system_high_adaptation_threshhold.Get(&high));
  EXPECT_EQ(high, 0.95f);
}

TEST_F(WebRtcVideoEngineTestFake, TestSetInvalidCpuThreshold) {
  EXPECT_TRUE(SetupEngine());
  float low, high;
  cricket::VideoOptions options_in, options_out;
  // Valid range is [0, 1].
  options_in.system_low_adaptation_threshhold.Set(-1.5f);
  options_in.system_high_adaptation_threshhold.Set(1.5f);
  EXPECT_TRUE(channel_->SetOptions(options_in));
  EXPECT_TRUE(channel_->GetOptions(&options_out));
  EXPECT_TRUE(options_out.system_low_adaptation_threshhold.Get(&low));
  EXPECT_EQ(low, 0.0f);
  EXPECT_TRUE(options_out.system_high_adaptation_threshhold.Get(&high));
  EXPECT_EQ(high, 1.0f);
}


TEST_F(WebRtcVideoEngineTestFake, ResetCodecOnScreencast) {
  EXPECT_TRUE(SetupEngine());
  cricket::VideoOptions options;
  options.video_noise_reduction.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));

  // Set send codec.
  cricket::VideoCodec codec(kVP8Codec);
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(codec);
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(123)));
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_EQ(1, vie_.num_set_send_codecs());

  webrtc::VideoCodec gcodec;
  memset(&gcodec, 0, sizeof(gcodec));
  int channel_num = vie_.GetLastChannel();
  EXPECT_EQ(0, vie_.GetSendCodec(channel_num, gcodec));
  EXPECT_TRUE(gcodec.codecSpecific.VP8.denoisingOn);

  // Send a screencast frame with the same size.
  // Verify that denoising is turned off.
  SendI420ScreencastFrame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_EQ(2, vie_.num_set_send_codecs());
  EXPECT_EQ(0, vie_.GetSendCodec(channel_num, gcodec));
  EXPECT_FALSE(gcodec.codecSpecific.VP8.denoisingOn);
}


TEST_F(WebRtcVideoEngineTestFake, DontRegisterDecoderIfFactoryIsNotGiven) {
  engine_.SetExternalDecoderFactory(NULL);
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVP8Codec);
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));

  EXPECT_EQ(0, vie_.GetNumExternalDecoderRegistered(channel_num));
}

TEST_F(WebRtcVideoEngineTestFake, RegisterDecoderIfFactoryIsGiven) {
  decoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecVP8);
  engine_.SetExternalDecoderFactory(&decoder_factory_);
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVP8Codec);
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));

  EXPECT_TRUE(vie_.ExternalDecoderRegistered(channel_num, 100));
  EXPECT_EQ(1, vie_.GetNumExternalDecoderRegistered(channel_num));
}

TEST_F(WebRtcVideoEngineTestFake, DontRegisterDecoderMultipleTimes) {
  decoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecVP8);
  engine_.SetExternalDecoderFactory(&decoder_factory_);
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVP8Codec);
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));

  EXPECT_TRUE(vie_.ExternalDecoderRegistered(channel_num, 100));
  EXPECT_EQ(1, vie_.GetNumExternalDecoderRegistered(channel_num));
  EXPECT_EQ(1, decoder_factory_.GetNumCreatedDecoders());

  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));
  EXPECT_EQ(1, vie_.GetNumExternalDecoderRegistered(channel_num));
  EXPECT_EQ(1, decoder_factory_.GetNumCreatedDecoders());
}

TEST_F(WebRtcVideoEngineTestFake, DontRegisterDecoderForNonVP8) {
  decoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecVP8);
  engine_.SetExternalDecoderFactory(&decoder_factory_);
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kRedCodec);
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));

  EXPECT_EQ(0, vie_.GetNumExternalDecoderRegistered(channel_num));
}

TEST_F(WebRtcVideoEngineTestFake, DontRegisterEncoderIfFactoryIsNotGiven) {
  engine_.SetExternalEncoderFactory(NULL);
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVP8Codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  EXPECT_EQ(0, vie_.GetNumExternalEncoderRegistered(channel_num));
}

TEST_F(WebRtcVideoEngineTestFake, RegisterEncoderIfFactoryIsGiven) {
  encoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecVP8, "VP8");
  engine_.SetExternalEncoderFactory(&encoder_factory_);
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVP8Codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrc)));

  EXPECT_TRUE(vie_.ExternalEncoderRegistered(channel_num, 100));
  EXPECT_EQ(1, vie_.GetNumExternalEncoderRegistered(channel_num));

  // Remove stream previously added to free the external encoder instance.
  EXPECT_TRUE(channel_->RemoveSendStream(kSsrc));
}

TEST_F(WebRtcVideoEngineTestFake, DontRegisterEncoderMultipleTimes) {
  encoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecVP8, "VP8");
  engine_.SetExternalEncoderFactory(&encoder_factory_);
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVP8Codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrc)));

  EXPECT_TRUE(vie_.ExternalEncoderRegistered(channel_num, 100));
  EXPECT_EQ(1, vie_.GetNumExternalEncoderRegistered(channel_num));
  EXPECT_EQ(1, encoder_factory_.GetNumCreatedEncoders());

  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_EQ(1, vie_.GetNumExternalEncoderRegistered(channel_num));
  EXPECT_EQ(1, encoder_factory_.GetNumCreatedEncoders());

  // Remove stream previously added to free the external encoder instance.
  EXPECT_TRUE(channel_->RemoveSendStream(kSsrc));
}

TEST_F(WebRtcVideoEngineTestFake, RegisterEncoderWithMultipleSendStreams) {
  encoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecVP8, "VP8");
  engine_.SetExternalEncoderFactory(&encoder_factory_);
  EXPECT_TRUE(SetupEngine());

  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVP8Codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_EQ(1, vie_.GetTotalNumExternalEncoderRegistered());

  // When we add the first stream (1234), it reuses the default send channel,
  // so it doesn't increase the registration count of external encoders.
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(1234)));
  EXPECT_EQ(1, vie_.GetTotalNumExternalEncoderRegistered());

  // When we add the second stream (2345), it creates a new channel and
  // increments the registration count.
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(2345)));
  EXPECT_EQ(2, vie_.GetTotalNumExternalEncoderRegistered());

  // At this moment the total registration count is two, but only one encoder
  // is registered per channel.
  int channel_num = vie_.GetLastChannel();
  EXPECT_EQ(1, vie_.GetNumExternalEncoderRegistered(channel_num));

  // Removing send streams decrements the registration count.
  EXPECT_TRUE(channel_->RemoveSendStream(1234));
  EXPECT_EQ(1, vie_.GetTotalNumExternalEncoderRegistered());

  // When we remove the last send stream, it also destroys the last send
  // channel and causes the registration count to drop to zero. It is a little
  // weird, but not a bug.
  EXPECT_TRUE(channel_->RemoveSendStream(2345));
  EXPECT_EQ(0, vie_.GetTotalNumExternalEncoderRegistered());
}

TEST_F(WebRtcVideoEngineTestFake, DontRegisterEncoderForNonVP8) {
  encoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecGeneric,
                                              "GENERIC");
  engine_.SetExternalEncoderFactory(&encoder_factory_);
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  // Note: unlike the SetRecvCodecs, we must set a valid video codec for
  // channel_->SetSendCodecs() to succeed.
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVP8Codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  EXPECT_EQ(0, vie_.GetNumExternalEncoderRegistered(channel_num));
}

// Test that NACK, PLI and REMB are enabled for external codec.
TEST_F(WebRtcVideoEngineTestFake, ExternalCodecFeedbackParams) {
  encoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecGeneric,
                                              "GENERIC");
  engine_.SetExternalEncoderFactory(&encoder_factory_);
  encoder_factory_.NotifyCodecsAvailable();
  EXPECT_TRUE(SetupEngine());

  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  // The external codec will appear at last.
  size_t pos = codecs.size() - 1;
  EXPECT_EQ("GENERIC", codecs[pos].name);
  VerifyCodecFeedbackParams(codecs[pos]);
}

// Test external codec with be added to the end of the supported codec list.
TEST_F(WebRtcVideoEngineTestFake, ExternalCodecAddedToTheEnd) {
  EXPECT_TRUE(SetupEngine());

  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  EXPECT_EQ("VP8", codecs[0].name);

  encoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecGeneric,
                                              "GENERIC");
  engine_.SetExternalEncoderFactory(&encoder_factory_);
  encoder_factory_.NotifyCodecsAvailable();

  codecs = engine_.codecs();
  cricket::VideoCodec internal_codec = codecs[0];
  cricket::VideoCodec external_codec = codecs[codecs.size() - 1];
  // The external codec will appear at last.
  EXPECT_EQ("GENERIC", external_codec.name);
  // The internal codec is preferred.
  EXPECT_GE(internal_codec.preference, external_codec.preference);
}

// Test that external codec with be ignored if it has the same name as one of
// the internal codecs.
TEST_F(WebRtcVideoEngineTestFake, ExternalCodecIgnored) {
  EXPECT_TRUE(SetupEngine());

  std::vector<cricket::VideoCodec> internal_codecs(engine_.codecs());
  EXPECT_EQ("VP8", internal_codecs[0].name);

  encoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecVP8, "VP8");
  engine_.SetExternalEncoderFactory(&encoder_factory_);
  encoder_factory_.NotifyCodecsAvailable();

  std::vector<cricket::VideoCodec> codecs = engine_.codecs();
  EXPECT_EQ("VP8", codecs[0].name);
  EXPECT_EQ(internal_codecs[0].height, codecs[0].height);
  EXPECT_EQ(internal_codecs[0].width, codecs[0].width);
  // Verify the last codec is not the external codec.
  EXPECT_NE("VP8", codecs[codecs.size() - 1].name);
}

TEST_F(WebRtcVideoEngineTestFake, UpdateEncoderCodecsAfterSetFactory) {
  engine_.SetExternalEncoderFactory(&encoder_factory_);
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  encoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecVP8, "VP8");
  encoder_factory_.NotifyCodecsAvailable();
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVP8Codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrc)));

  EXPECT_TRUE(vie_.ExternalEncoderRegistered(channel_num, 100));
  EXPECT_EQ(1, vie_.GetNumExternalEncoderRegistered(channel_num));
  EXPECT_EQ(1, encoder_factory_.GetNumCreatedEncoders());

  // Remove stream previously added to free the external encoder instance.
  EXPECT_TRUE(channel_->RemoveSendStream(kSsrc));
}

// Tests that OnReadyToSend will be propagated into ViE.
TEST_F(WebRtcVideoEngineTestFake, OnReadyToSend) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  EXPECT_TRUE(vie_.GetIsTransmitting(channel_num));

  channel_->OnReadyToSend(false);
  EXPECT_FALSE(vie_.GetIsTransmitting(channel_num));

  channel_->OnReadyToSend(true);
  EXPECT_TRUE(vie_.GetIsTransmitting(channel_num));
}

#if 0
TEST_F(WebRtcVideoEngineTestFake, CaptureFrameTimestampToNtpTimestamp) {
  EXPECT_TRUE(SetupEngine());
  int capture_id = vie_.GetCaptureId(vie_.GetLastChannel());

  // Set send codec.
  cricket::VideoCodec codec(kVP8Codec);
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(codec);
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(123)));
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  EXPECT_TRUE(channel_->SetSend(true));

  int64 timestamp = time(NULL) * talk_base::kNumNanosecsPerSec;
  SendI420ScreencastFrameWithTimestamp(
      kVP8Codec.width, kVP8Codec.height, timestamp);
  EXPECT_EQ(talk_base::UnixTimestampNanosecsToNtpMillisecs(timestamp),
      vie_.GetCaptureLastTimestamp(capture_id));

  SendI420ScreencastFrameWithTimestamp(kVP8Codec.width, kVP8Codec.height, 0);
  EXPECT_EQ(0, vie_.GetCaptureLastTimestamp(capture_id));
}
#endif

/////////////////////////
// Tests with real ViE //
/////////////////////////

// Tests that we can find codecs by name or id.
TEST_F(WebRtcVideoEngineTest, FindCodec) {
  // We should not need to init engine in order to get codecs.
  const std::vector<cricket::VideoCodec>& c = engine_.codecs();
  EXPECT_EQ(4U, c.size());

  cricket::VideoCodec vp8(104, "VP8", 320, 200, 30, 0);
  EXPECT_TRUE(engine_.FindCodec(vp8));

  cricket::VideoCodec vp8_ci(104, "vp8", 320, 200, 30, 0);
  EXPECT_TRUE(engine_.FindCodec(vp8));

  cricket::VideoCodec vp8_diff_fr_diff_pref(104, "VP8", 320, 200, 50, 50);
  EXPECT_TRUE(engine_.FindCodec(vp8_diff_fr_diff_pref));

  cricket::VideoCodec vp8_diff_id(95, "VP8", 320, 200, 30, 0);
  EXPECT_FALSE(engine_.FindCodec(vp8_diff_id));
  vp8_diff_id.id = 97;
  EXPECT_TRUE(engine_.FindCodec(vp8_diff_id));

  cricket::VideoCodec vp8_diff_res(104, "VP8", 320, 111, 30, 0);
  EXPECT_FALSE(engine_.FindCodec(vp8_diff_res));

  // PeerConnection doesn't negotiate the resolution at this point.
  // Test that FindCodec can handle the case when width/height is 0.
  cricket::VideoCodec vp8_zero_res(104, "VP8", 0, 0, 30, 0);
  EXPECT_TRUE(engine_.FindCodec(vp8_zero_res));

  cricket::VideoCodec red(101, "RED", 0, 0, 30, 0);
  EXPECT_TRUE(engine_.FindCodec(red));

  cricket::VideoCodec red_ci(101, "red", 0, 0, 30, 0);
  EXPECT_TRUE(engine_.FindCodec(red));

  cricket::VideoCodec fec(102, "ULPFEC", 0, 0, 30, 0);
  EXPECT_TRUE(engine_.FindCodec(fec));

  cricket::VideoCodec fec_ci(102, "ulpfec", 0, 0, 30, 0);
  EXPECT_TRUE(engine_.FindCodec(fec));

  cricket::VideoCodec rtx(96, "rtx", 0, 0, 30, 0);
  EXPECT_TRUE(engine_.FindCodec(rtx));
}

TEST_F(WebRtcVideoEngineTest, RtxCodecHasAptSet) {
  std::vector<cricket::VideoCodec>::const_iterator it;
  bool apt_checked = false;
  for (it = engine_.codecs().begin(); it != engine_.codecs().end(); ++it) {
    if (_stricmp(cricket::kRtxCodecName, it->name.c_str()) && it->id != 96) {
      continue;
    }
    int apt;
    EXPECT_TRUE(it->GetParam("apt", &apt));
    EXPECT_EQ(100, apt);
    apt_checked = true;
  }
  EXPECT_TRUE(apt_checked);
}

TEST_F(WebRtcVideoEngineTest, StartupShutdown) {
  EXPECT_TRUE(engine_.Init(talk_base::Thread::Current()));
  engine_.Terminate();
}

TEST_PRE_VIDEOENGINE_INIT(WebRtcVideoEngineTest, ConstrainNewCodec)
TEST_POST_VIDEOENGINE_INIT(WebRtcVideoEngineTest, ConstrainNewCodec)

TEST_PRE_VIDEOENGINE_INIT(WebRtcVideoEngineTest, ConstrainRunningCodec)
TEST_POST_VIDEOENGINE_INIT(WebRtcVideoEngineTest, ConstrainRunningCodec)

// TODO(juberti): Figure out why ViE is munging the COM refcount.
#ifdef WIN32
TEST_F(WebRtcVideoEngineTest, DISABLED_CheckCoInitialize) {
  Base::CheckCoInitialize();
}
#endif

TEST_F(WebRtcVideoEngineTest, CreateChannel) {
  EXPECT_TRUE(engine_.Init(talk_base::Thread::Current()));
  cricket::VideoMediaChannel* channel = engine_.CreateChannel(NULL);
  EXPECT_TRUE(channel != NULL);
  delete channel;
}

TEST_F(WebRtcVideoMediaChannelTest, SetRecvCodecs) {
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVP8Codec);
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));
}
TEST_F(WebRtcVideoMediaChannelTest, SetRecvCodecsWrongPayloadType) {
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVP8Codec);
  codecs[0].id = 99;
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));
}
TEST_F(WebRtcVideoMediaChannelTest, SetRecvCodecsUnsupportedCodec) {
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVP8Codec);
  codecs.push_back(cricket::VideoCodec(101, "VP1", 640, 400, 30, 0));
  EXPECT_FALSE(channel_->SetRecvCodecs(codecs));
}

TEST_F(WebRtcVideoMediaChannelTest, SetSend) {
  Base::SetSend();
}
TEST_F(WebRtcVideoMediaChannelTest, SetSendWithoutCodecs) {
  Base::SetSendWithoutCodecs();
}
TEST_F(WebRtcVideoMediaChannelTest, SetSendSetsTransportBufferSizes) {
  Base::SetSendSetsTransportBufferSizes();
}

TEST_F(WebRtcVideoMediaChannelTest, SendAndReceiveVp8Vga) {
  SendAndReceive(cricket::VideoCodec(100, "VP8", 640, 400, 30, 0));
}
TEST_F(WebRtcVideoMediaChannelTest, SendAndReceiveVp8Qvga) {
  SendAndReceive(cricket::VideoCodec(100, "VP8", 320, 200, 30, 0));
}
TEST_F(WebRtcVideoMediaChannelTest, SendAndReceiveH264SvcQqvga) {
  SendAndReceive(cricket::VideoCodec(100, "VP8", 160, 100, 30, 0));
}
TEST_F(WebRtcVideoMediaChannelTest, SendManyResizeOnce) {
  SendManyResizeOnce();
}

TEST_F(WebRtcVideoMediaChannelTest, SendVp8HdAndReceiveAdaptedVp8Vga) {
  EXPECT_TRUE(channel_->SetCapturer(kSsrc, NULL));
  channel_->UpdateAspectRatio(1280, 720);
  video_capturer_.reset(new cricket::FakeVideoCapturer);
  const std::vector<cricket::VideoFormat>* formats =
      video_capturer_->GetSupportedFormats();
  cricket::VideoFormat capture_format_hd = (*formats)[0];
  EXPECT_EQ(cricket::CS_RUNNING, video_capturer_->Start(capture_format_hd));
  EXPECT_TRUE(channel_->SetCapturer(kSsrc, video_capturer_.get()));

  // Capture format HD -> adapt (OnOutputFormatRequest VGA) -> VGA.
  cricket::VideoCodec codec(100, "VP8", 1280, 720, 30, 0);
  EXPECT_TRUE(SetOneCodec(codec));
  codec.width /= 2;
  codec.height /= 2;
  EXPECT_TRUE(channel_->SetSendStreamFormat(kSsrc, cricket::VideoFormat(
      codec.width, codec.height,
      cricket::VideoFormat::FpsToInterval(codec.framerate),
      cricket::FOURCC_ANY)));
  EXPECT_TRUE(SetSend(true));
  EXPECT_TRUE(channel_->SetRender(true));
  EXPECT_EQ(0, renderer_.num_rendered_frames());
  EXPECT_TRUE(SendFrame());
  EXPECT_FRAME_WAIT(1, codec.width, codec.height, kTimeout);
}

// TODO(juberti): Fix this test to tolerate missing stats.
TEST_F(WebRtcVideoMediaChannelTest, DISABLED_GetStats) {
  Base::GetStats();
}

// TODO(juberti): Fix this test to tolerate missing stats.
TEST_F(WebRtcVideoMediaChannelTest, DISABLED_GetStatsMultipleRecvStreams) {
  Base::GetStatsMultipleRecvStreams();
}

TEST_F(WebRtcVideoMediaChannelTest, GetStatsMultipleSendStreams) {
  Base::GetStatsMultipleSendStreams();
}

TEST_F(WebRtcVideoMediaChannelTest, SetSendBandwidth) {
  Base::SetSendBandwidth();
}
TEST_F(WebRtcVideoMediaChannelTest, SetSendSsrc) {
  Base::SetSendSsrc();
}
TEST_F(WebRtcVideoMediaChannelTest, SetSendSsrcAfterSetCodecs) {
  Base::SetSendSsrcAfterSetCodecs();
}

TEST_F(WebRtcVideoMediaChannelTest, SetRenderer) {
  Base::SetRenderer();
}

TEST_F(WebRtcVideoMediaChannelTest, AddRemoveRecvStreams) {
  Base::AddRemoveRecvStreams();
}

TEST_F(WebRtcVideoMediaChannelTest, AddRemoveRecvStreamAndRender) {
  Base::AddRemoveRecvStreamAndRender();
}

TEST_F(WebRtcVideoMediaChannelTest, AddRemoveRecvStreamsNoConference) {
  Base::AddRemoveRecvStreamsNoConference();
}

TEST_F(WebRtcVideoMediaChannelTest, AddRemoveSendStreams) {
  Base::AddRemoveSendStreams();
}

TEST_F(WebRtcVideoMediaChannelTest, SimulateConference) {
  Base::SimulateConference();
}

TEST_F(WebRtcVideoMediaChannelTest, AddRemoveCapturer) {
  Base::AddRemoveCapturer();
}

TEST_F(WebRtcVideoMediaChannelTest, RemoveCapturerWithoutAdd) {
  Base::RemoveCapturerWithoutAdd();
}

TEST_F(WebRtcVideoMediaChannelTest, AddRemoveCapturerMultipleSources) {
  Base::AddRemoveCapturerMultipleSources();
}

// This test verifies DSCP settings are properly applied on video media channel.
TEST_F(WebRtcVideoMediaChannelTest, TestSetDscpOptions) {
  talk_base::scoped_ptr<cricket::FakeNetworkInterface> network_interface(
      new cricket::FakeNetworkInterface);
  channel_->SetInterface(network_interface.get());
  cricket::VideoOptions options;
  options.dscp.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  EXPECT_EQ(talk_base::DSCP_AF41, network_interface->dscp());
  // Verify previous value is not modified if dscp option is not set.
  cricket::VideoOptions options1;
  EXPECT_TRUE(channel_->SetOptions(options1));
  EXPECT_EQ(talk_base::DSCP_AF41, network_interface->dscp());
  options.dscp.Set(false);
  EXPECT_TRUE(channel_->SetOptions(options));
  EXPECT_EQ(talk_base::DSCP_DEFAULT, network_interface->dscp());
  channel_->SetInterface(NULL);
}


TEST_F(WebRtcVideoMediaChannelTest, SetOptionsSucceedsWhenSending) {
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));

  // Verify SetOptions returns true on a different options.
  cricket::VideoOptions options2;
  options2.adapt_input_to_cpu_usage.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options2));

  // Set send codecs on the channel and start sending.
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVP8Codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_TRUE(channel_->SetSend(true));

  // Verify SetOptions returns true if channel is already sending.
  cricket::VideoOptions options3;
  options3.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options3));
}

// Tests empty StreamParams is rejected.
TEST_F(WebRtcVideoMediaChannelTest, RejectEmptyStreamParams) {
  Base::RejectEmptyStreamParams();
}


TEST_F(WebRtcVideoMediaChannelTest, AdaptResolution16x10) {
  Base::AdaptResolution16x10();
}

TEST_F(WebRtcVideoMediaChannelTest, AdaptResolution4x3) {
  Base::AdaptResolution4x3();
}

TEST_F(WebRtcVideoMediaChannelTest, MuteStream) {
  Base::MuteStream();
}

TEST_F(WebRtcVideoMediaChannelTest, MultipleSendStreams) {
  Base::MultipleSendStreams();
}

// TODO(juberti): Restore this test once we support sending 0 fps.
TEST_F(WebRtcVideoMediaChannelTest, DISABLED_AdaptDropAllFrames) {
  Base::AdaptDropAllFrames();
}
// TODO(juberti): Understand why we get decode errors on this test.
TEST_F(WebRtcVideoMediaChannelTest, DISABLED_AdaptFramerate) {
  Base::AdaptFramerate();
}

TEST_F(WebRtcVideoMediaChannelTest, SetSendStreamFormat0x0) {
  Base::SetSendStreamFormat0x0();
}

// TODO(zhurunz): Fix the flakey test.
TEST_F(WebRtcVideoMediaChannelTest, DISABLED_SetSendStreamFormat) {
  Base::SetSendStreamFormat();
}

TEST_F(WebRtcVideoMediaChannelTest, TwoStreamsSendAndReceive) {
  Base::TwoStreamsSendAndReceive(cricket::VideoCodec(100, "VP8", 640, 400, 30,
                                                     0));
}

TEST_F(WebRtcVideoMediaChannelTest, TwoStreamsReUseFirstStream) {
  Base::TwoStreamsReUseFirstStream(cricket::VideoCodec(100, "VP8", 640, 400, 30,
                                                       0));
}
