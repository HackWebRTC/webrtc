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

#include <algorithm>
#include "talk/media/base/constants.h"
#include "talk/media/base/fakenetworkinterface.h"
#include "talk/media/base/fakevideorenderer.h"
#include "talk/media/base/mediachannel.h"
#include "talk/media/base/testutils.h"
#include "talk/media/base/videoadapter.h"
#include "talk/media/base/videoengine_unittest.h"
#include "talk/media/webrtc/fakewebrtcvideoengine.h"
#include "talk/media/webrtc/simulcast.h"
#include "talk/media/webrtc/webrtcvideoengine.h"
#include "talk/media/webrtc/webrtcvideoframe.h"
#include "talk/media/webrtc/webrtcvoiceengine.h"
#include "talk/session/media/mediasession.h"
#include "webrtc/base/fakecpumonitor.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/stream.h"
#include "webrtc/system_wrappers/interface/trace.h"

// Tests for the WebRtcVideoEngine/VideoChannel code.

using cricket::kRtpTimestampOffsetHeaderExtension;
using cricket::kRtpAbsoluteSenderTimeHeaderExtension;

static const cricket::VideoCodec kVP8Codec720p(100, "VP8", 1280, 720, 30, 0);
static const cricket::VideoCodec kVP8Codec360p(100, "VP8", 640, 360, 30, 0);
static const cricket::VideoCodec kVP8Codec270p(100, "VP8", 480, 270, 30, 0);

static const cricket::VideoCodec kVP8Codec(100, "VP8", 640, 400, 30, 0);
static const cricket::VideoCodec kH264Codec(127, "H264", 640, 400, 30, 0);
static const cricket::VideoCodec kRedCodec(101, "red", 0, 0, 0, 0);
static const cricket::VideoCodec kUlpFecCodec(102, "ulpfec", 0, 0, 0, 0);
static const cricket::VideoCodec* const kVideoCodecs[] = {
    &kVP8Codec,
    &kRedCodec,
    &kUlpFecCodec
};

static const unsigned int kStartBandwidthKbps = 300;
static const unsigned int kMinBandwidthKbps = 30;
static const unsigned int kMaxBandwidthKbps = 2000;

static const uint32 kSsrcs1[] = {1};
static const uint32 kSsrcs2[] = {1, 2};
static const uint32 kSsrcs3[] = {1, 2, 3};
static const uint32 kRtxSsrcs1[] = {4};
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
        cpu_monitor_(new rtc::FakeCpuMonitor(
            rtc::Thread::Current())),
        engine_(NULL,  // cricket::WebRtcVoiceEngine
                new FakeViEWrapper(&vie_), cpu_monitor_),
        channel_(NULL),
        voice_channel_(NULL),
        last_error_(cricket::VideoMediaChannel::ERROR_NONE) {
  }
  bool SetupEngine() {
    bool result = engine_.Init(rtc::Thread::Current());
    if (result) {
      channel_ = engine_.CreateChannel(cricket::VideoOptions(), voice_channel_);
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
  void TestSetSendRtpHeaderExtensions(const std::string& ext) {
    EXPECT_TRUE(SetupEngine());
    int channel_num = vie_.GetLastChannel();

    // Verify extensions are off by default.
    EXPECT_EQ(-1, vie_.GetSendRtpExtensionId(channel_num, ext));

    // Enable extension.
    const int id = 1;
    std::vector<cricket::RtpHeaderExtension> extensions;
    extensions.push_back(cricket::RtpHeaderExtension(ext, id));

    // Verify the send extension id.
    EXPECT_TRUE(channel_->SetSendRtpHeaderExtensions(extensions));
    EXPECT_EQ(id, vie_.GetSendRtpExtensionId(channel_num, ext));
    // Verify call with same set of extensions returns true.
    EXPECT_TRUE(channel_->SetSendRtpHeaderExtensions(extensions));
    EXPECT_EQ(id, vie_.GetSendRtpExtensionId(channel_num, ext));

    // Add a new send stream and verify the extension is set.
    // The first send stream to occupy the default channel.
    EXPECT_TRUE(
        channel_->AddSendStream(cricket::StreamParams::CreateLegacy(123)));
    EXPECT_TRUE(
        channel_->AddSendStream(cricket::StreamParams::CreateLegacy(234)));
    int new_send_channel_num = vie_.GetLastChannel();
    EXPECT_NE(channel_num, new_send_channel_num);
    EXPECT_EQ(id, vie_.GetSendRtpExtensionId(new_send_channel_num, ext));

    // Remove the extension id.
    std::vector<cricket::RtpHeaderExtension> empty_extensions;
    EXPECT_TRUE(channel_->SetSendRtpHeaderExtensions(empty_extensions));
    EXPECT_EQ(-1, vie_.GetSendRtpExtensionId(channel_num, ext));
    EXPECT_EQ(-1, vie_.GetSendRtpExtensionId(new_send_channel_num, ext));
  }
  void TestSetRecvRtpHeaderExtensions(const std::string& ext) {
    EXPECT_TRUE(SetupEngine());
    int channel_num = vie_.GetLastChannel();

    // Verify extensions are off by default.
    EXPECT_EQ(-1, vie_.GetReceiveRtpExtensionId(channel_num, ext));

    // Enable extension.
    const int id = 2;
    std::vector<cricket::RtpHeaderExtension> extensions;
    extensions.push_back(cricket::RtpHeaderExtension(ext, id));

    // Verify receive extension id.
    EXPECT_TRUE(channel_->SetRecvRtpHeaderExtensions(extensions));
    EXPECT_EQ(id, vie_.GetReceiveRtpExtensionId(channel_num, ext));
    // Verify call with same set of extensions returns true.
    EXPECT_TRUE(channel_->SetRecvRtpHeaderExtensions(extensions));
    EXPECT_EQ(id, vie_.GetReceiveRtpExtensionId(channel_num, ext));

    // Add a new receive stream and verify the extension is set.
    // The first send stream to occupy the default channel.
    EXPECT_TRUE(
        channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(345)));
    EXPECT_TRUE(
        channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(456)));
    int new_recv_channel_num = vie_.GetLastChannel();
    EXPECT_NE(channel_num, new_recv_channel_num);
    EXPECT_EQ(id, vie_.GetReceiveRtpExtensionId(new_recv_channel_num, ext));

    // Remove the extension id.
    std::vector<cricket::RtpHeaderExtension> empty_extensions;
    EXPECT_TRUE(channel_->SetRecvRtpHeaderExtensions(empty_extensions));
    EXPECT_EQ(-1, vie_.GetReceiveRtpExtensionId(channel_num, ext));
    EXPECT_EQ(-1, vie_.GetReceiveRtpExtensionId(new_recv_channel_num, ext));
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
    EXPECT_EQ(std::min(start_bitrate, max_bitrate), gcodec.startBitrate);
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
  rtc::FakeCpuMonitor* cpu_monitor_;
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
  EXPECT_TRUE(engine_.Init(rtc::Thread::Current()));
  EXPECT_TRUE(vie_.IsInited());
  engine_.Terminate();
}

// Tests that webrtc logs are logged when they should be.
TEST_F(WebRtcVideoEngineTest, WebRtcShouldLog) {
  const char webrtc_log[] = "WebRtcVideoEngineTest.WebRtcShouldLog";
  EXPECT_TRUE(engine_.Init(rtc::Thread::Current()));
  engine_.SetLogging(rtc::LS_INFO, "");
  std::string str;
  rtc::StringStream stream(str);
  rtc::LogMessage::AddLogToStream(&stream, rtc::LS_INFO);
  EXPECT_EQ(rtc::LS_INFO, rtc::LogMessage::GetLogToStream(&stream));
  webrtc::Trace::Add(webrtc::kTraceStateInfo, webrtc::kTraceUndefined, 0,
                     webrtc_log);
  rtc::Thread::Current()->ProcessMessages(100);
  rtc::LogMessage::RemoveLogToStream(&stream);
  // Access |str| after LogMessage is done with it to avoid data racing.
  EXPECT_NE(std::string::npos, str.find(webrtc_log));
}

// Tests that webrtc logs are not logged when they should't be.
TEST_F(WebRtcVideoEngineTest, WebRtcShouldNotLog) {
  const char webrtc_log[] = "WebRtcVideoEngineTest.WebRtcShouldNotLog";
  EXPECT_TRUE(engine_.Init(rtc::Thread::Current()));
  // WebRTC should never be logged lower than LS_INFO.
  engine_.SetLogging(rtc::LS_WARNING, "");
  std::string str;
  rtc::StringStream stream(str);
  // Make sure that WebRTC is not logged, even at lowest severity
  rtc::LogMessage::AddLogToStream(&stream, rtc::LS_SENSITIVE);
  EXPECT_EQ(rtc::LS_SENSITIVE,
            rtc::LogMessage::GetLogToStream(&stream));
  webrtc::Trace::Add(webrtc::kTraceStateInfo, webrtc::kTraceUndefined, 0,
                     webrtc_log);
  rtc::Thread::Current()->ProcessMessages(10);
  EXPECT_EQ(std::string::npos, str.find(webrtc_log));
  rtc::LogMessage::RemoveLogToStream(&stream);
}

// Tests that we can create and destroy a channel.
TEST_F(WebRtcVideoEngineTestFake, CreateChannel) {
  EXPECT_TRUE(engine_.Init(rtc::Thread::Current()));
  channel_ = engine_.CreateChannel(cricket::VideoOptions(), voice_channel_);
  EXPECT_TRUE(channel_ != NULL);
  EXPECT_EQ(1, engine_.GetNumOfChannels());
  delete channel_;
  channel_ = NULL;
  EXPECT_EQ(0, engine_.GetNumOfChannels());
}

// Tests that we properly handle failures in CreateChannel.
TEST_F(WebRtcVideoEngineTestFake, CreateChannelFail) {
  vie_.set_fail_create_channel(true);
  EXPECT_TRUE(engine_.Init(rtc::Thread::Current()));
  channel_ = engine_.CreateChannel(cricket::VideoOptions(), voice_channel_);
  EXPECT_TRUE(channel_ == NULL);
}

// Tests that we properly handle failures in AllocateExternalCaptureDevice.
TEST_F(WebRtcVideoEngineTestFake, AllocateExternalCaptureDeviceFail) {
  vie_.set_fail_alloc_capturer(true);
  EXPECT_TRUE(engine_.Init(rtc::Thread::Current()));
  channel_ = engine_.CreateChannel(cricket::VideoOptions(), voice_channel_);
  EXPECT_TRUE(channel_ == NULL);
}

// Test that we apply our default codecs properly.
TEST_F(WebRtcVideoEngineTestFake, SetSendCodecs) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height);
  EXPECT_TRUE(vie_.GetHybridNackFecStatus(channel_num));
  EXPECT_FALSE(vie_.GetNackStatus(channel_num));
  EXPECT_EQ(1, vie_.GetNumSetSendCodecs());
  // TODO(juberti): Check RTCP, PLI, TMMBR.
}

TEST_F(WebRtcVideoEngineTestFake, SetSendCodecsBeforeFrameRegistersTinyFormat) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  VerifyVP8SendCodec(channel_num, 176, 144);
}

// Test that ViE Channel doesn't call SetSendCodec again if same codec is tried
// to apply.
TEST_F(WebRtcVideoEngineTestFake, DontResetSetSendCodec) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height);
  EXPECT_TRUE(vie_.GetHybridNackFecStatus(channel_num));
  EXPECT_FALSE(vie_.GetNackStatus(channel_num));
  EXPECT_EQ(1, vie_.GetNumSetSendCodecs());
  // Try setting same code again.
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  // Since it's exact same codec which is already set, media channel shouldn't
  // send the codec to ViE.
  EXPECT_EQ(1, vie_.GetNumSetSendCodecs());
}

TEST_F(WebRtcVideoEngineTestFake, SetSendCodecsWithMinMaxBitrate) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  codecs[0].params[cricket::kCodecParamMinBitrate] = "10";
  codecs[0].params[cricket::kCodecParamMaxBitrate] = "20";
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  VerifyVP8SendCodec(
      channel_num, kVP8Codec.width, kVP8Codec.height, 0, 20, 10, 20);

  cricket::VideoCodec codec;
  EXPECT_TRUE(channel_->GetSendCodec(&codec));
  EXPECT_EQ("10", codec.params[cricket::kCodecParamMinBitrate]);
  EXPECT_EQ("20", codec.params[cricket::kCodecParamMaxBitrate]);
}

TEST_F(WebRtcVideoEngineTestFake, SetSendCodecsWithStartBitrate) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  codecs[0].params[cricket::kCodecParamStartBitrate] = "450";
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  VerifyVP8SendCodec(channel_num,
                     kVP8Codec.width,
                     kVP8Codec.height,
                     0,
                     kMaxBandwidthKbps,
                     kMinBandwidthKbps,
                     450);

  cricket::VideoCodec codec;
  EXPECT_TRUE(channel_->GetSendCodec(&codec));
  EXPECT_EQ("450", codec.params[cricket::kCodecParamStartBitrate]);
}

TEST_F(WebRtcVideoEngineTestFake, SetSendCodecsWithMinMaxStartBitrate) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  codecs[0].params[cricket::kCodecParamMinBitrate] = "10";
  codecs[0].params[cricket::kCodecParamMaxBitrate] = "20";
  codecs[0].params[cricket::kCodecParamStartBitrate] = "14";
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  VerifyVP8SendCodec(
      channel_num, kVP8Codec.width, kVP8Codec.height, 0, 20, 10, 14);

  cricket::VideoCodec codec;
  EXPECT_TRUE(channel_->GetSendCodec(&codec));
  EXPECT_EQ("10", codec.params[cricket::kCodecParamMinBitrate]);
  EXPECT_EQ("20", codec.params[cricket::kCodecParamMaxBitrate]);
  EXPECT_EQ("14", codec.params[cricket::kCodecParamStartBitrate]);
}

TEST_F(WebRtcVideoEngineTestFake, SetSendCodecsWithMinMaxBitrateInvalid) {
  EXPECT_TRUE(SetupEngine());
  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  codecs[0].params[cricket::kCodecParamMinBitrate] = "30";
  codecs[0].params[cricket::kCodecParamMaxBitrate] = "20";
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_FALSE(channel_->SetSendCodecs(codecs));
}

TEST_F(WebRtcVideoEngineTestFake, SetSendCodecsWithLargeMinMaxBitrate) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  codecs[0].params[cricket::kCodecParamMinBitrate] = "1000";
  codecs[0].params[cricket::kCodecParamMaxBitrate] = "3000";
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  VerifyVP8SendCodec(
      channel_num, kVP8Codec.width, kVP8Codec.height, 0, 3000, 1000,
      1000);
}

TEST_F(WebRtcVideoEngineTestFake, SetSendCodecsWithMaxQuantization) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  codecs[0].params[cricket::kCodecParamMaxQuantization] = "21";
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  VerifyVP8SendCodec(channel_num,
                     kVP8Codec.width,
                     kVP8Codec.height,
                     0,
                     kMaxBandwidthKbps,
                     kMinBandwidthKbps,
                     300,
                     30,
                     21);

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
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
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

TEST_F(WebRtcVideoEngineTestFake, MaxBitrateResetWithConferenceMode) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  codecs[0].params[cricket::kCodecParamMinBitrate] = "10";
  codecs[0].params[cricket::kCodecParamMaxBitrate] = "20";
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
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
      kMaxBandwidthKbps, 10, kStartBandwidthKbps);
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
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  VerifyVP8SendCodec(send_channel, kVP8Codec.width, kVP8Codec.height, 0,
                     kMaxBandwidthKbps, kMinBandwidthKbps,
                     kStartBandwidthKbps);
  EXPECT_EQ(0, vie_.StartSend(send_channel));

  // Increase the send bitrate and verify it is used as start bitrate.
  const unsigned int kIncreasedSendBitrateBps = 768000;
  vie_.SetSendBitrates(send_channel, kIncreasedSendBitrateBps, 0, 0);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  VerifyVP8SendCodec(send_channel, kVP8Codec.width, kVP8Codec.height, 0,
                     kMaxBandwidthKbps, kMinBandwidthKbps,
                     kIncreasedSendBitrateBps / 1000);

  // Never set a start bitrate higher than the max bitrate.
  vie_.SetSendBitrates(send_channel, kMaxBandwidthKbps + 500, 0, 0);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  VerifyVP8SendCodec(send_channel, kVP8Codec.width, kVP8Codec.height, 0,
                     kMaxBandwidthKbps, kMinBandwidthKbps,
                     kStartBandwidthKbps);

  // Use the default start bitrate if the send bitrate is lower.
  vie_.SetSendBitrates(send_channel, kStartBandwidthKbps - 50, 0,
                       0);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  VerifyVP8SendCodec(send_channel, kVP8Codec.width, kVP8Codec.height, 0,
                     kMaxBandwidthKbps, kMinBandwidthKbps,
                     kStartBandwidthKbps);
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
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
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
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
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
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
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
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
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

// Test that we set our inbound RTX codecs properly.
TEST_F(WebRtcVideoEngineTestFake, SetRecvCodecsWithRtx) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  std::vector<cricket::VideoCodec> codecs;
  cricket::VideoCodec rtx_codec(96, "rtx", 0, 0, 0, 0);
  codecs.push_back(rtx_codec);
  // Should fail since there's no associated payload type set.
  EXPECT_FALSE(channel_->SetRecvCodecs(codecs));

  codecs[0].SetParam("apt", 97);
  // Should still fail since the we don't support RTX on this APT.
  EXPECT_FALSE(channel_->SetRecvCodecs(codecs));

  codecs[0].SetParam("apt", kVP8Codec.id);
  // Should still fail since the associated payload type is unknown.
  EXPECT_FALSE(channel_->SetRecvCodecs(codecs));

  codecs.push_back(kVP8Codec);
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));

  webrtc::VideoCodec wcodec;
  // Should not have been registered as a WebRTC codec.
  EXPECT_TRUE(engine_.ConvertFromCricketVideoCodec(rtx_codec, &wcodec));
  EXPECT_STREQ("rtx", wcodec.plName);
  EXPECT_FALSE(vie_.ReceiveCodecRegistered(channel_num, wcodec));

  // The RTX payload type should have been set.
  EXPECT_EQ(rtx_codec.id, vie_.GetRtxRecvPayloadType(channel_num));
}

// Test that RTX packets are routed to the default video channel if
// there's only one recv stream.
TEST_F(WebRtcVideoEngineTestFake, TestReceiveRtxOneStream) {
  EXPECT_TRUE(SetupEngine());

  // Setup one channel with an associated RTX stream.
  cricket::StreamParams params =
    cricket::StreamParams::CreateLegacy(kSsrcs1[0]);
  params.AddFidSsrc(kSsrcs1[0], kRtxSsrcs1[0]);
  EXPECT_TRUE(channel_->AddRecvStream(params));
  int channel_num = vie_.GetLastChannel();
  EXPECT_EQ(static_cast<int>(kRtxSsrcs1[0]),
            vie_.GetRemoteRtxSsrc(channel_num));

  // Register codecs.
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(kVP8Codec720p);
  cricket::VideoCodec rtx_codec(96, "rtx", 0, 0, 0, 0);
  rtx_codec.SetParam("apt", kVP8Codec.id);
  codec_list.push_back(rtx_codec);
  EXPECT_TRUE(channel_->SetRecvCodecs(codec_list));

  // Construct a fake RTX packet and verify that it is passed to the
  // right WebRTC channel.
  const size_t kDataLength = 12;
  uint8_t data[kDataLength];
  memset(data, 0, sizeof(data));
  data[0] = 0x80;
  data[1] = rtx_codec.id;
  rtc::SetBE32(&data[8], kRtxSsrcs1[0]);
  rtc::Buffer packet(data, kDataLength);
  rtc::PacketTime packet_time;
  channel_->OnPacketReceived(&packet, packet_time);
  EXPECT_EQ(rtx_codec.id, vie_.GetLastRecvdPayloadType(channel_num));
}

// Verify we don't crash when inserting packets after removing the default
// receive channel.
TEST_F(WebRtcVideoEngineTestFake, TestReceiveRtxWithRemovedDefaultChannel) {
  EXPECT_TRUE(SetupEngine());

  // Setup one channel with an associated RTX stream.
  cricket::StreamParams params =
    cricket::StreamParams::CreateLegacy(kSsrcs1[0]);
  params.AddFidSsrc(kSsrcs1[0], kRtxSsrcs1[0]);
  EXPECT_TRUE(channel_->AddRecvStream(params));
  int channel_num = vie_.GetLastChannel();
  EXPECT_EQ(static_cast<int>(kRtxSsrcs1[0]),
            vie_.GetRemoteRtxSsrc(channel_num));

  // Register codecs.
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(kVP8Codec720p);
  cricket::VideoCodec rtx_codec(96, "rtx", 0, 0, 0, 0);
  rtx_codec.SetParam("apt", kVP8Codec.id);
  codec_list.push_back(rtx_codec);
  EXPECT_TRUE(channel_->SetRecvCodecs(codec_list));

  // Construct a fake RTX packet and verify that it is passed to the
  // right WebRTC channel.
  const size_t kDataLength = 12;
  uint8_t data[kDataLength];
  memset(data, 0, sizeof(data));
  data[0] = 0x80;
  data[1] = rtx_codec.id;
  rtc::SetBE32(&data[8], kRtxSsrcs1[0]);
  rtc::Buffer packet(data, kDataLength);
  rtc::PacketTime packet_time;
  channel_->OnPacketReceived(&packet, packet_time);
  EXPECT_EQ(rtx_codec.id, vie_.GetLastRecvdPayloadType(channel_num));

  // Remove the default channel and insert one more packet.
  EXPECT_TRUE(channel_->RemoveRecvStream(kSsrcs1[0]));
  channel_->OnPacketReceived(&packet, packet_time);
}

// Test that RTX packets are routed to the correct video channel.
TEST_F(WebRtcVideoEngineTestFake, TestReceiveRtxThreeStreams) {
  EXPECT_TRUE(SetupEngine());

  // Setup three channels with associated RTX streams.
  int channel_num[ARRAY_SIZE(kSsrcs3)];
  for (size_t i = 0; i < ARRAY_SIZE(kSsrcs3); ++i) {
    cricket::StreamParams params =
      cricket::StreamParams::CreateLegacy(kSsrcs3[i]);
    params.AddFidSsrc(kSsrcs3[i], kRtxSsrcs3[i]);
    EXPECT_TRUE(channel_->AddRecvStream(params));
    channel_num[i] = vie_.GetLastChannel();
  }

  // Register codecs.
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(kVP8Codec720p);
  cricket::VideoCodec rtx_codec(96, "rtx", 0, 0, 0, 0);
  rtx_codec.SetParam("apt", kVP8Codec.id);
  codec_list.push_back(rtx_codec);
  EXPECT_TRUE(channel_->SetRecvCodecs(codec_list));

  // Construct a fake RTX packet and verify that it is passed to the
  // right WebRTC channel.
  const size_t kDataLength = 12;
  uint8_t data[kDataLength];
  memset(data, 0, sizeof(data));
  data[0] = 0x80;
  data[1] = rtx_codec.id;
  rtc::SetBE32(&data[8], kRtxSsrcs3[1]);
  rtc::Buffer packet(data, kDataLength);
  rtc::PacketTime packet_time;
  channel_->OnPacketReceived(&packet, packet_time);
  EXPECT_NE(rtx_codec.id, vie_.GetLastRecvdPayloadType(channel_num[0]));
  EXPECT_EQ(rtx_codec.id, vie_.GetLastRecvdPayloadType(channel_num[1]));
  EXPECT_NE(rtx_codec.id, vie_.GetLastRecvdPayloadType(channel_num[2]));
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
                                            MAKE_VECTOR(kRtxSsrcs1))));
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
TEST_F(WebRtcVideoEngineTestFake, SendRtpTimestampOffsetHeaderExtensions) {
  TestSetSendRtpHeaderExtensions(kRtpTimestampOffsetHeaderExtension);
}
TEST_F(WebRtcVideoEngineTestFake, RecvRtpTimestampOffsetHeaderExtensions) {
  TestSetRecvRtpHeaderExtensions(kRtpTimestampOffsetHeaderExtension);
}

// Test support for absolute send time header extension.
TEST_F(WebRtcVideoEngineTestFake, SendAbsoluteSendTimeHeaderExtensions) {
  TestSetSendRtpHeaderExtensions(kRtpAbsoluteSenderTimeHeaderExtension);
}
TEST_F(WebRtcVideoEngineTestFake, RecvAbsoluteSendTimeHeaderExtensions) {
  TestSetRecvRtpHeaderExtensions(kRtpAbsoluteSenderTimeHeaderExtension);
}

TEST_F(WebRtcVideoEngineTestFake, LeakyBucketTest) {
  EXPECT_TRUE(SetupEngine());

  // Verify this is on by default.
  EXPECT_TRUE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(1)));
  int first_send_channel = vie_.GetLastChannel();
  EXPECT_TRUE(vie_.GetTransmissionSmoothingStatus(first_send_channel));

  // Add a new send stream and verify leaky bucket is enabled.
  EXPECT_TRUE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(3)));
  int second_send_channel = vie_.GetLastChannel();
  EXPECT_NE(first_send_channel, second_send_channel);
  EXPECT_TRUE(vie_.GetTransmissionSmoothingStatus(second_send_channel));
}

// Verify that SuspendBelowMinBitrate is enabled if it is set in the options.
TEST_F(WebRtcVideoEngineTestFake, SuspendBelowMinBitrateTest) {
  EXPECT_TRUE(SetupEngine());

  // Verify this is off by default.
  EXPECT_TRUE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(1)));
  int first_send_channel = vie_.GetLastChannel();
  EXPECT_FALSE(vie_.GetSuspendBelowMinBitrateStatus(first_send_channel));

  // Enable the experiment and verify.
  cricket::VideoOptions options;
  options.suspend_below_min_bitrate.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  EXPECT_TRUE(vie_.GetSuspendBelowMinBitrateStatus(first_send_channel));

  // Add a new send stream and verify suspend_below_min_bitrate is enabled.
  EXPECT_TRUE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(2)));
  int second_send_channel = vie_.GetLastChannel();
  EXPECT_NE(first_send_channel, second_send_channel);
  EXPECT_TRUE(vie_.GetSuspendBelowMinBitrateStatus(second_send_channel));
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
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  channel_->SetSendCodecs(engine_.codecs());

  int first_send_channel = vie_.GetLastChannel();
  EXPECT_EQ(0, vie_.GetSenderTargetDelay(first_send_channel));
  EXPECT_EQ(0, vie_.GetReceiverTargetDelay(first_send_channel));

  cricket::VideoOptions options1;
  options1.buffered_mode_latency.Set(100);
  EXPECT_TRUE(channel_->SetOptions(options1));
  EXPECT_EQ(100, vie_.GetSenderTargetDelay(first_send_channel));
  EXPECT_EQ(100, vie_.GetReceiverTargetDelay(first_send_channel));
  EXPECT_TRUE(vie_.GetTransmissionSmoothingStatus(first_send_channel));

  int kBoostedStartBandwidthKbps = 1000;
  cricket::VideoOptions options2;
  options2.video_start_bitrate.Set(kBoostedStartBandwidthKbps);
  EXPECT_TRUE(channel_->SetOptions(options2));
  // Check that start bitrate has changed to the new value.
  VerifyVP8SendCodec(first_send_channel, kVP8Codec.width, kVP8Codec.height, 0,
      kMaxBandwidthKbps, kMinBandwidthKbps, kBoostedStartBandwidthKbps);
  // The buffered_mode_latency should still take effect.
  EXPECT_EQ(100, vie_.GetSenderTargetDelay(first_send_channel));
  EXPECT_EQ(100, vie_.GetReceiverTargetDelay(first_send_channel));
}

TEST_F(WebRtcVideoEngineTestFake, SetCpuOveruseOptionsWithCaptureJitterMethod) {
  EXPECT_TRUE(SetupEngine());

  // Verify this is off by default.
  EXPECT_TRUE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(1)));
  int first_send_channel = vie_.GetLastChannel();
  webrtc::CpuOveruseOptions cpu_option =
      vie_.GetCpuOveruseOptions(first_send_channel);
  EXPECT_EQ(0, cpu_option.low_capture_jitter_threshold_ms);
  EXPECT_EQ(0, cpu_option.high_capture_jitter_threshold_ms);
  EXPECT_FALSE(cpu_option.enable_capture_jitter_method);
  EXPECT_FALSE(cpu_option.enable_encode_usage_method);

  // Set low and high threshold and verify that cpu options are set.
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  options.cpu_underuse_threshold.Set(10);
  options.cpu_overuse_threshold.Set(20);
  EXPECT_TRUE(channel_->SetOptions(options));
  cpu_option = vie_.GetCpuOveruseOptions(first_send_channel);
  EXPECT_EQ(10, cpu_option.low_capture_jitter_threshold_ms);
  EXPECT_EQ(20, cpu_option.high_capture_jitter_threshold_ms);
  EXPECT_TRUE(cpu_option.enable_capture_jitter_method);
  EXPECT_FALSE(cpu_option.enable_encode_usage_method);

  // Add a receive channel and verify that cpu options are not set.
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  int recv_channel_num = vie_.GetLastChannel();
  EXPECT_NE(first_send_channel, recv_channel_num);
  cpu_option = vie_.GetCpuOveruseOptions(recv_channel_num);
  EXPECT_EQ(0, cpu_option.low_capture_jitter_threshold_ms);
  EXPECT_EQ(0, cpu_option.high_capture_jitter_threshold_ms);
  EXPECT_FALSE(cpu_option.enable_capture_jitter_method);
  EXPECT_FALSE(cpu_option.enable_encode_usage_method);

  // Add a new send stream and verify that cpu options are set from start.
  EXPECT_TRUE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(3)));
  int second_send_channel = vie_.GetLastChannel();
  EXPECT_NE(first_send_channel, second_send_channel);
  cpu_option = vie_.GetCpuOveruseOptions(second_send_channel);
  EXPECT_EQ(10, cpu_option.low_capture_jitter_threshold_ms);
  EXPECT_EQ(20, cpu_option.high_capture_jitter_threshold_ms);
  EXPECT_TRUE(cpu_option.enable_capture_jitter_method);
  EXPECT_FALSE(cpu_option.enable_encode_usage_method);
}

TEST_F(WebRtcVideoEngineTestFake, SetInvalidCpuOveruseThresholds) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(1)));
  int channel_num = vie_.GetLastChannel();

  // Only low threshold set. Verify that cpu options are not set.
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  options.cpu_underuse_threshold.Set(10);
  EXPECT_TRUE(channel_->SetOptions(options));
  webrtc::CpuOveruseOptions cpu_option = vie_.GetCpuOveruseOptions(channel_num);
  EXPECT_EQ(0, cpu_option.low_capture_jitter_threshold_ms);
  EXPECT_EQ(0, cpu_option.high_capture_jitter_threshold_ms);
  EXPECT_FALSE(cpu_option.enable_capture_jitter_method);
  EXPECT_FALSE(cpu_option.enable_encode_usage_method);

  // Set high threshold to a negative value. Verify that options are not set.
  options.cpu_overuse_threshold.Set(-1);
  EXPECT_TRUE(channel_->SetOptions(options));
  cpu_option = vie_.GetCpuOveruseOptions(channel_num);
  EXPECT_EQ(0, cpu_option.low_capture_jitter_threshold_ms);
  EXPECT_EQ(0, cpu_option.high_capture_jitter_threshold_ms);
  EXPECT_FALSE(cpu_option.enable_capture_jitter_method);
  EXPECT_FALSE(cpu_option.enable_encode_usage_method);

  // Low and high threshold valid. Verify that cpu options are set.
  options.cpu_overuse_threshold.Set(20);
  EXPECT_TRUE(channel_->SetOptions(options));
  cpu_option = vie_.GetCpuOveruseOptions(channel_num);
  EXPECT_EQ(10, cpu_option.low_capture_jitter_threshold_ms);
  EXPECT_EQ(20, cpu_option.high_capture_jitter_threshold_ms);
  EXPECT_TRUE(cpu_option.enable_capture_jitter_method);
  EXPECT_FALSE(cpu_option.enable_encode_usage_method);
}

TEST_F(WebRtcVideoEngineTestFake, SetCpuOveruseOptionsWithEncodeUsageMethod) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(1)));
  int first_send_channel = vie_.GetLastChannel();

  // Set low and high threshold and enable encode usage method.
  // Verify that cpu options are set.
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  options.cpu_underuse_threshold.Set(10);
  options.cpu_overuse_threshold.Set(20);
  options.cpu_overuse_encode_usage.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  webrtc::CpuOveruseOptions cpu_option =
      vie_.GetCpuOveruseOptions(first_send_channel);
  EXPECT_EQ(10, cpu_option.low_encode_usage_threshold_percent);
  EXPECT_EQ(20, cpu_option.high_encode_usage_threshold_percent);
  EXPECT_FALSE(cpu_option.enable_capture_jitter_method);
  EXPECT_TRUE(cpu_option.enable_encode_usage_method);
  // Verify that optional encode rsd thresholds are not set.
  EXPECT_EQ(-1, cpu_option.low_encode_time_rsd_threshold);
  EXPECT_EQ(-1, cpu_option.high_encode_time_rsd_threshold);

  // Add a new send stream and verify that cpu options are set from start.
  EXPECT_TRUE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(3)));
  int second_send_channel = vie_.GetLastChannel();
  EXPECT_NE(first_send_channel, second_send_channel);
  cpu_option = vie_.GetCpuOveruseOptions(second_send_channel);
  EXPECT_EQ(10, cpu_option.low_encode_usage_threshold_percent);
  EXPECT_EQ(20, cpu_option.high_encode_usage_threshold_percent);
  EXPECT_FALSE(cpu_option.enable_capture_jitter_method);
  EXPECT_TRUE(cpu_option.enable_encode_usage_method);
  // Verify that optional encode rsd thresholds are not set.
  EXPECT_EQ(-1, cpu_option.low_encode_time_rsd_threshold);
  EXPECT_EQ(-1, cpu_option.high_encode_time_rsd_threshold);
}

TEST_F(WebRtcVideoEngineTestFake, SetCpuOveruseOptionsWithEncodeRsdThresholds) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(1)));
  int first_send_channel = vie_.GetLastChannel();

  // Set optional encode rsd thresholds and verify cpu options.
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  options.cpu_underuse_threshold.Set(10);
  options.cpu_overuse_threshold.Set(20);
  options.cpu_underuse_encode_rsd_threshold.Set(30);
  options.cpu_overuse_encode_rsd_threshold.Set(40);
  options.cpu_overuse_encode_usage.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  webrtc::CpuOveruseOptions cpu_option =
      vie_.GetCpuOveruseOptions(first_send_channel);
  EXPECT_EQ(10, cpu_option.low_encode_usage_threshold_percent);
  EXPECT_EQ(20, cpu_option.high_encode_usage_threshold_percent);
  EXPECT_FALSE(cpu_option.enable_capture_jitter_method);
  EXPECT_TRUE(cpu_option.enable_encode_usage_method);
  EXPECT_EQ(30, cpu_option.low_encode_time_rsd_threshold);
  EXPECT_EQ(40, cpu_option.high_encode_time_rsd_threshold);

  // Add a new send stream and verify that cpu options are set from start.
  EXPECT_TRUE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(3)));
  int second_send_channel = vie_.GetLastChannel();
  EXPECT_NE(first_send_channel, second_send_channel);
  cpu_option = vie_.GetCpuOveruseOptions(second_send_channel);
  EXPECT_EQ(10, cpu_option.low_encode_usage_threshold_percent);
  EXPECT_EQ(20, cpu_option.high_encode_usage_threshold_percent);
  EXPECT_FALSE(cpu_option.enable_capture_jitter_method);
  EXPECT_TRUE(cpu_option.enable_encode_usage_method);
  EXPECT_EQ(30, cpu_option.low_encode_time_rsd_threshold);
  EXPECT_EQ(40, cpu_option.high_encode_time_rsd_threshold);
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

// Test that adding/removing stream with 0 ssrc should fail (and not crash).
// For crbug/351699 and 350988.
TEST_F(WebRtcVideoEngineTestFake, AddRemoveRecvStreamWith0Ssrc) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  EXPECT_FALSE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(0)));
  EXPECT_FALSE(channel_->RemoveRecvStream(0));
  EXPECT_TRUE(channel_->RemoveRecvStream(1));
}

TEST_F(WebRtcVideoEngineTestFake, AddRemoveSendStreamWith0Ssrc) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(1)));
  EXPECT_FALSE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(0)));
  EXPECT_FALSE(channel_->RemoveSendStream(0));
  EXPECT_TRUE(channel_->RemoveSendStream(1));
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
  // Verify receiving is also started.
  EXPECT_TRUE(vie_.GetReceive(channel_num));

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
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));
  EXPECT_TRUE(channel_->SetMaxSendBandwidth(cricket::kAutoBandwidth));
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height);
}

// Test that we set bandwidth properly when using auto with upper bound.
TEST_F(WebRtcVideoEngineTestFake, SetBandwidthCapped) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));
  EXPECT_TRUE(channel_->SetMaxSendBandwidth(768000));
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height, 0, 768U);
}

// Test that we reduce the start bandwidth when the requested max is less than
// the default start bandwidth.
TEST_F(WebRtcVideoEngineTestFake, SetMaxBandwidthBelowDefaultStart) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));
  int max_bandwidth_kbps = (kMinBandwidthKbps + kStartBandwidthKbps) / 2;
  EXPECT_TRUE(channel_->SetMaxSendBandwidth(max_bandwidth_kbps * 1000));
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height, 0,
      max_bandwidth_kbps, kMinBandwidthKbps, max_bandwidth_kbps);
}

// Test that we reduce the min bandwidth when the requested max is less than
// the min bandwidth.
TEST_F(WebRtcVideoEngineTestFake, SetMaxBandwidthBelowMin) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));
  int max_bandwidth_kbps = kMinBandwidthKbps / 2;
  EXPECT_TRUE(channel_->SetMaxSendBandwidth(max_bandwidth_kbps * 1000));
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height, 0,
      max_bandwidth_kbps, max_bandwidth_kbps, max_bandwidth_kbps);
}

// Test that the start bandwidth can be controlled by VideoOptions.
TEST_F(WebRtcVideoEngineTestFake, SetStartBandwidthOption) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height, 0,
      kMaxBandwidthKbps, kMinBandwidthKbps, kStartBandwidthKbps);

  // Set the start bitrate option.
  unsigned int kBoostedStartBandwidthKbps = 1000;
  ASSERT_NE(kStartBandwidthKbps, kBoostedStartBandwidthKbps);
  cricket::VideoOptions options;
  options.video_start_bitrate.Set(kBoostedStartBandwidthKbps);
  EXPECT_TRUE(channel_->SetOptions(options));

  // Check that start bitrate has changed to the new value.
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height, 0,
      kMaxBandwidthKbps, kMinBandwidthKbps, kBoostedStartBandwidthKbps);
}

// Test that SetMaxSendBandwidth works as expected in conference mode.
TEST_F(WebRtcVideoEngineTestFake, SetBandwidthInConference) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height);

  // Set send bandwidth.
  EXPECT_TRUE(channel_->SetMaxSendBandwidth(768000));

  // Verify that the max bitrate has changed.
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height, 0,
                     768, kMinBandwidthKbps, kStartBandwidthKbps);
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
  EXPECT_TRUE(channel_->SetMaxSendBandwidth(111000));
  EXPECT_TRUE(channel_->SetSend(true));

  SendI420ScreencastFrame(kVP8Codec.width, kVP8Codec.height);
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height, 0, 111);
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

  // Both channels should have started receiving after created.
  EXPECT_TRUE(vie_.GetReceive(channel0));
  EXPECT_TRUE(vie_.GetReceive(channel1));

  // Set send codec.
  std::vector<cricket::VideoCodec> codecs;
  cricket::VideoCodec send_codec(100, "VP8", 640, 480, 30, 0);
  codecs.push_back(send_codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_TRUE(vie_.GetSend(channel0));
  EXPECT_TRUE(vie_.GetSend(channel1));

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

TEST_F(WebRtcVideoEngineTestFake, SendReceiveBitratesStats) {
  EXPECT_TRUE(SetupEngine());
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(1)));
  int first_send_channel = vie_.GetLastChannel();
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(2)));
  int second_send_channel = vie_.GetLastChannel();
  cricket::VideoCodec codec(kVP8Codec720p);
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));

  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::StreamParams::CreateLegacy(3)));
  int first_receive_channel = vie_.GetLastChannel();
  EXPECT_NE(first_send_channel, first_receive_channel);
  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::StreamParams::CreateLegacy(4)));
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
  EXPECT_EQ(0, vie_.StartSend(first_send_channel));
  int send_video_bitrate = 800;
  int send_fec_bitrate = 100;
  int send_nack_bitrate = 20;
  int send_total_bitrate = send_video_bitrate + send_fec_bitrate +
      send_nack_bitrate;
  int send_bandwidth = 1900;
  vie_.SetSendBitrates(first_send_channel, send_video_bitrate, send_fec_bitrate,
                       send_nack_bitrate);
  vie_.SetSendBandwidthEstimate(first_send_channel, send_bandwidth);

  EXPECT_EQ(0, vie_.StartReceive(first_receive_channel));
  int receive_bandwidth = 600;
  vie_.SetReceiveBandwidthEstimate(first_receive_channel, receive_bandwidth);

  info.Clear();
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.bw_estimations.size());
  ASSERT_EQ(send_video_bitrate, info.bw_estimations[0].actual_enc_bitrate);
  ASSERT_EQ(send_total_bitrate, info.bw_estimations[0].transmit_bitrate);
  ASSERT_EQ(send_nack_bitrate, info.bw_estimations[0].retransmit_bitrate);
  ASSERT_EQ(send_bandwidth, info.bw_estimations[0].available_send_bandwidth);
  ASSERT_EQ(receive_bandwidth, info.bw_estimations[0].available_recv_bandwidth);
  ASSERT_EQ(send_video_bitrate, info.bw_estimations[0].target_enc_bitrate);

  // Start receiving on the second channel and verify received rate.
  EXPECT_EQ(0, vie_.StartSend(second_send_channel));
  vie_.SetSendBitrates(second_send_channel,
                       send_video_bitrate,
                       send_fec_bitrate,
                       send_nack_bitrate);
  EXPECT_EQ(0, vie_.StartReceive(second_receive_channel));

  info.Clear();
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.bw_estimations.size());
  ASSERT_EQ(2 * send_video_bitrate, info.bw_estimations[0].actual_enc_bitrate);
  ASSERT_EQ(2 * send_total_bitrate, info.bw_estimations[0].transmit_bitrate);
  ASSERT_EQ(2 * send_nack_bitrate, info.bw_estimations[0].retransmit_bitrate);
  ASSERT_EQ(send_bandwidth, info.bw_estimations[0].available_send_bandwidth);
  ASSERT_EQ(receive_bandwidth, info.bw_estimations[0].available_recv_bandwidth);
  ASSERT_EQ(2 * send_video_bitrate, info.bw_estimations[0].target_enc_bitrate);
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
  EXPECT_EQ(1, vie_.GetNumSetSendCodecs());

  webrtc::VideoCodec gcodec;
  memset(&gcodec, 0, sizeof(gcodec));
  int channel_num = vie_.GetLastChannel();
  EXPECT_EQ(0, vie_.GetSendCodec(channel_num, gcodec));
  EXPECT_TRUE(gcodec.codecSpecific.VP8.denoisingOn);

  // Send a screencast frame with the same size.
  // Verify that denoising is turned off.
  SendI420ScreencastFrame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_EQ(2, vie_.GetNumSetSendCodecs());
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

  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_EQ(1, vie_.GetNumExternalEncoderRegistered(channel_num));

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
  EXPECT_TRUE(SetupEngine());

  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  // The external codec will appear at last.
  size_t pos = codecs.size() - 1;
  EXPECT_EQ("GENERIC", codecs[pos].name);
  VerifyCodecFeedbackParams(codecs[pos]);
}

// Test external codec will be added to the end of the supported codec list.
TEST_F(WebRtcVideoEngineTestFake, ExternalCodecAddedToTheEnd) {
  encoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecGeneric,
                                              "GENERIC");
  engine_.SetExternalEncoderFactory(&encoder_factory_);
  EXPECT_TRUE(SetupEngine());

  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  EXPECT_EQ("VP8", codecs[0].name);

  codecs = engine_.codecs();
  cricket::VideoCodec internal_codec = codecs[0];
  cricket::VideoCodec external_codec = codecs[codecs.size() - 1];
  // The external codec will appear at last.
  EXPECT_EQ("GENERIC", external_codec.name);
  // The internal codec is preferred.
  EXPECT_GE(internal_codec.preference, external_codec.preference);
}

// Test that external codecs that we support internally are not added as
// duplicate entries to the codecs list.
TEST_F(WebRtcVideoEngineTestFake, ExternalCodecIgnored) {
  encoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecVP8, "VP8");
  engine_.SetExternalEncoderFactory(&encoder_factory_);
  EXPECT_TRUE(SetupEngine());

  std::vector<cricket::VideoCodec> internal_codecs(engine_.codecs());
  EXPECT_EQ("VP8", internal_codecs[0].name);

  std::vector<cricket::VideoCodec> codecs = engine_.codecs();
  EXPECT_EQ("VP8", codecs[0].name);
  EXPECT_EQ(internal_codecs[0].height, codecs[0].height);
  EXPECT_EQ(internal_codecs[0].width, codecs[0].width);
  // Verify the last codec is not the external codec.
  EXPECT_NE("VP8", codecs[codecs.size() - 1].name);
}

TEST_F(WebRtcVideoEngineTestFake, SetSendCodecsWithExternalH264) {
  encoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecH264, "H264");
  engine_.SetExternalEncoderFactory(&encoder_factory_);
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kH264Codec);
  cricket::VideoCodec rtx_codec(96, "rtx", 0, 0, 0, 0);
  rtx_codec.SetParam("apt", kH264Codec.id);
  codecs.push_back(rtx_codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  EXPECT_EQ(96, vie_.GetRtxSendPayloadType(channel_num));

  cricket::StreamParams params =
    cricket::StreamParams::CreateLegacy(kSsrcs1[0]);
  params.AddFidSsrc(kSsrcs1[0], kRtxSsrcs1[0]);
  EXPECT_TRUE(channel_->AddSendStream(params));

  EXPECT_EQ(1, vie_.GetNumSsrcs(channel_num));
  EXPECT_EQ(1, vie_.GetNumRtxSsrcs(channel_num));
  EXPECT_EQ(static_cast<int>(kRtxSsrcs1[0]), vie_.GetRtxSsrc(channel_num, 0));

  EXPECT_TRUE(vie_.ExternalEncoderRegistered(channel_num, 127));
  EXPECT_EQ(1, vie_.GetNumExternalEncoderRegistered(channel_num));
  EXPECT_EQ(1, encoder_factory_.GetNumCreatedEncoders());

  EXPECT_TRUE(channel_->RemoveSendStream(kSsrcs1[0]));
}

TEST_F(WebRtcVideoEngineTestFake, SetSendCodecsWithVP8AndExternalH264) {
  encoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecH264, "H264");
  engine_.SetExternalEncoderFactory(&encoder_factory_);
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kH264Codec);
  cricket::VideoCodec rtx_codec(96, "rtx", 0, 0, 0, 0);
  rtx_codec.SetParam("apt", kH264Codec.id);
  codecs.push_back(rtx_codec);
  codecs.push_back(kVP8Codec);
  cricket::VideoCodec rtx_codec2(97, "rtx", 0, 0, 0, 0);
  rtx_codec2.SetParam("apt", kVP8Codec.id);
  codecs.push_back(rtx_codec2);

  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  // The first matched codec should be set, i.e., H.264.

  EXPECT_EQ(96, vie_.GetRtxSendPayloadType(channel_num));

  cricket::StreamParams params =
    cricket::StreamParams::CreateLegacy(kSsrcs1[0]);
  params.AddFidSsrc(kSsrcs1[0], kRtxSsrcs1[0]);
  EXPECT_TRUE(channel_->AddSendStream(params));

  EXPECT_EQ(1, vie_.GetNumSsrcs(channel_num));
  EXPECT_EQ(1, vie_.GetNumRtxSsrcs(channel_num));
  EXPECT_EQ(static_cast<int>(kRtxSsrcs1[0]), vie_.GetRtxSsrc(channel_num, 0));

  EXPECT_TRUE(vie_.ExternalEncoderRegistered(channel_num, 127));
  EXPECT_EQ(1, vie_.GetNumExternalEncoderRegistered(channel_num));
  EXPECT_EQ(1, encoder_factory_.GetNumCreatedEncoders());

  EXPECT_TRUE(channel_->RemoveSendStream(kSsrcs1[0]));
}

TEST_F(WebRtcVideoEngineTestFake, SetRecvCodecsWithExternalH264) {
  // WebRtcVideoEngine assumes that if we have encode support for a codec, we
  // also have decode support. It doesn't support decode only support. Therefore
  // we here have to register both an encoder and a decoder factory with H264
  // support, to be able to test the decoder factory.
  encoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecH264, "H264");
  decoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecH264);
  EXPECT_TRUE(SetupEngine());
  engine_.SetExternalEncoderFactory(&encoder_factory_);
  engine_.SetExternalDecoderFactory(&decoder_factory_);
  int channel_num = vie_.GetLastChannel();

  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kH264Codec);
  cricket::VideoCodec rtx_codec(96, "rtx", 0, 0, 0, 0);
  rtx_codec.SetParam("apt", kH264Codec.id);
  codecs.push_back(rtx_codec);
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));

  EXPECT_EQ(96, vie_.GetRtxRecvPayloadType(channel_num));

  cricket::StreamParams params =
    cricket::StreamParams::CreateLegacy(kSsrcs1[0]);
    params.AddFidSsrc(kSsrcs1[0], kRtxSsrcs1[0]);
  EXPECT_TRUE(channel_->AddRecvStream(params));

  EXPECT_EQ(1, vie_.GetNumSsrcs(channel_num));
  EXPECT_EQ(static_cast<int>(kRtxSsrcs1[0]),
            vie_.GetRemoteRtxSsrc(channel_num));

  EXPECT_TRUE(vie_.ExternalDecoderRegistered(channel_num, 127));
  EXPECT_EQ(1, vie_.GetNumExternalDecoderRegistered(channel_num));
  EXPECT_EQ(1, decoder_factory_.GetNumCreatedDecoders());

  EXPECT_TRUE(channel_->RemoveRecvStream(kSsrcs1[0]));
}

TEST_F(WebRtcVideoEngineTestFake, SetRecvCodecsWithVP8AndExternalH264) {
  encoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecH264, "H264");
  decoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecH264);
  EXPECT_TRUE(SetupEngine());
  engine_.SetExternalEncoderFactory(&encoder_factory_);
  engine_.SetExternalDecoderFactory(&decoder_factory_);
  int channel_num = vie_.GetLastChannel();

  std::vector<cricket::VideoCodec> codecs;
  cricket::VideoCodec rtx_codec(97, "rtx", 0, 0, 0, 0);
  rtx_codec.SetParam("apt", kH264Codec.id);
  codecs.push_back(kH264Codec);
  codecs.push_back(rtx_codec);

  cricket::VideoCodec rtx_codec2(96, "rtx", 0, 0, 0, 0);
  rtx_codec2.SetParam("apt", kVP8Codec.id);
  codecs.push_back(kVP8Codec);
  codecs.push_back(rtx_codec);
  // Should fail since WebRTC only supports one RTX codec at a time.
  EXPECT_FALSE(channel_->SetRecvCodecs(codecs));

  codecs.pop_back();

  // One RTX codec should be fine.
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));

  // The RTX payload type should have been set.
  EXPECT_EQ(rtx_codec.id, vie_.GetRtxRecvPayloadType(channel_num));
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

  int64 timestamp = time(NULL) * rtc::kNumNanosecsPerSec;
  SendI420ScreencastFrameWithTimestamp(
      kVP8Codec.width, kVP8Codec.height, timestamp);
  EXPECT_EQ(rtc::UnixTimestampNanosecsToNtpMillisecs(timestamp),
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
  // FindCodec ignores the codec size.
  // Test that FindCodec can accept uncommon codec size.
  EXPECT_TRUE(engine_.FindCodec(vp8_diff_res));

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
  rtx.SetParam("apt", kVP8Codec.id);
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
  EXPECT_TRUE(engine_.Init(rtc::Thread::Current()));
  engine_.Terminate();
}

TEST_PRE_VIDEOENGINE_INIT(WebRtcVideoEngineTest, ConstrainNewCodec2)
TEST_POST_VIDEOENGINE_INIT(WebRtcVideoEngineTest, ConstrainNewCodec2)

// TODO(juberti): Figure out why ViE is munging the COM refcount.
#ifdef WIN32
TEST_F(WebRtcVideoEngineTest, DISABLED_CheckCoInitialize) {
  Base::CheckCoInitialize();
}
#endif

TEST_F(WebRtcVideoEngineTest, CreateChannel) {
  EXPECT_TRUE(engine_.Init(rtc::Thread::Current()));
  cricket::VideoMediaChannel* channel =
      engine_.CreateChannel(cricket::VideoOptions(), NULL);
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

TEST_F(WebRtcVideoMediaChannelTest, GetRtpSendTimeExtension) {
  // Enable RTP timestamp extension.
  const int id = 12;
  std::vector<cricket::RtpHeaderExtension> extensions;
  extensions.push_back(cricket::RtpHeaderExtension(
      "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time", id));

  // Verify the send extension id.
  EXPECT_TRUE(channel_->SetSendRtpHeaderExtensions(extensions));
  EXPECT_EQ(id, channel_->GetRtpSendTimeExtnId());
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
TEST_F(WebRtcVideoMediaChannelTest, SendAndReceiveVp8Qqvga) {
  SendAndReceive(cricket::VideoCodec(100, "VP8", 160, 100, 30, 0));
}
TEST_F(WebRtcVideoMediaChannelTest, SendManyResizeOnce) {
  SendManyResizeOnce();
}

TEST_F(WebRtcVideoMediaChannelTest, DISABLED_SendVp8HdAndReceiveAdaptedVp8Vga) {
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
  EXPECT_TRUE(SetSend(true));
  EXPECT_TRUE(channel_->SetRender(true));
  EXPECT_EQ(0, renderer_.num_rendered_frames());
  EXPECT_TRUE(SendFrame());
  EXPECT_FRAME_WAIT(1, codec.width, codec.height, kTimeout);
}

TEST_F(WebRtcVideoMediaChannelTest, GetStats) {
  Base::GetStats();
}

TEST_F(WebRtcVideoMediaChannelTest, GetStatsMultipleRecvStreams) {
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
  rtc::scoped_ptr<cricket::FakeNetworkInterface> network_interface(
      new cricket::FakeNetworkInterface);
  channel_->SetInterface(network_interface.get());
  cricket::VideoOptions options;
  options.dscp.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  EXPECT_EQ(rtc::DSCP_AF41, network_interface->dscp());
  // Verify previous value is not modified if dscp option is not set.
  cricket::VideoOptions options1;
  EXPECT_TRUE(channel_->SetOptions(options1));
  EXPECT_EQ(rtc::DSCP_AF41, network_interface->dscp());
  options.dscp.Set(false);
  EXPECT_TRUE(channel_->SetOptions(options));
  EXPECT_EQ(rtc::DSCP_DEFAULT, network_interface->dscp());
  channel_->SetInterface(NULL);
}

TEST_F(WebRtcVideoMediaChannelTest, HighAspectHighHeightCapturer) {
  Base::HighAspectHighHeightCapturer();
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

TEST_F(WebRtcVideoMediaChannelTest, DISABLED_TwoStreamsSendAndUnsignalledRecv) {
  Base::TwoStreamsSendAndUnsignalledRecv(cricket::VideoCodec(100, "VP8", 640,
                                                             400, 30, 0));
}

TEST_F(WebRtcVideoMediaChannelTest,
       TwoStreamsSendAndFailUnsignalledRecv) {
  Base::TwoStreamsSendAndFailUnsignalledRecv(
      cricket::VideoCodec(100, "VP8", 640, 400, 30, 0));
}

TEST_F(WebRtcVideoMediaChannelTest,
       TwoStreamsSendAndFailUnsignalledRecvInOneToOne) {
  Base::TwoStreamsSendAndFailUnsignalledRecvInOneToOne(
      cricket::VideoCodec(100, "VP8", 640, 400, 30, 0));
}

TEST_F(WebRtcVideoMediaChannelTest,
       TwoStreamsAddAndRemoveUnsignalledRecv) {
  Base::TwoStreamsAddAndRemoveUnsignalledRecv(cricket::VideoCodec(100, "VP8",
                                                                  640, 400, 30,
                                                                  0));
}

// Test that sequence number are not reset if stopping and then
// resuming a stream.
TEST_F(WebRtcVideoMediaChannelTest, DontResetSequenceNumbers) {
  cricket::VideoCodec codec = DefaultCodec();
  EXPECT_TRUE(SetOneCodec(codec));

  uint16_t seq_before =
      engine_.vie()
          ->rtp()
          ->GetRtpStateForSsrc(channel_->GetDefaultChannelId(), kSsrc)
          .sequence_number;

  // Deactive.
  EXPECT_TRUE(channel_->RemoveSendStream(kSsrc));
  EXPECT_TRUE(SetOneCodec(codec));

  // Reactivate.
  EXPECT_TRUE(channel_->AddSendStream(DefaultSendStreamParams()));
  EXPECT_TRUE(SetOneCodec(codec));

  // Sequence number should now have changed.
  uint16_t seq_after =
      engine_.vie()
          ->rtp()
          ->GetRtpStateForSsrc(channel_->GetDefaultChannelId(), kSsrc)
          .sequence_number;

  EXPECT_EQ(seq_before, seq_after);
}

static const unsigned int kNumberOfTemporalLayers = 1;
static const unsigned int kSimulcastNumberOfTemporalLayers = 3;
static const unsigned int kSimStream0Bitrate = 100;
static const unsigned int kSimStream0MaxBitrateBoosted = 200;
static const unsigned int kSimStream0MaxBitrateXd = 200;
static const unsigned int kSimStream0TargetBitrateBoosted = 150;
static const unsigned int kSimStream0TargetBitrateXd = 150;
static const unsigned int kSimStream1Bitrate = 350;
static const unsigned int kSimStream1MaxBitrateBoosted = 450;
static const unsigned int kSimStream1MaxBitrateXd = 450;
static const unsigned int kSimStream1TargetBitrateBoosted = 350;
static const unsigned int kSimStream1TargetBitrateXd = 350;
static const unsigned int kSimStream2Bitrate = 500;
static const unsigned int kSimStream2MaxBitrateBoosted = 700;
static const unsigned int kSimStream2MaxBitrateXd = 700;
static const unsigned int kSimStream2TargetBitrateBoosted = 500;
static const unsigned int kSimStream2TargetBitrateXd = 500;
static const unsigned int kSimStream3Bitrate = 900;
static const unsigned int kSimStream3MaxBitrateBoosted = 900;
static const unsigned int kSimStream3MaxBitrateXd = 900;
static const unsigned int kSimStream3TargetBitrateBoosted = 900;
static const unsigned int kSimStream3TargetBitrateXd = 900;
static const unsigned int kSimStream4Bitrate = 1200;
static const unsigned int kSimStream4MaxBitrateBoosted = 1200;
static const unsigned int kSimStream4MaxBitrateXd = 2500;
static const unsigned int kSimStream4TargetBitrateBoosted = 1200;
static const unsigned int kSimStream4TargetBitrateXd = 2500;

struct SimulcastFormat {
  unsigned int width;
  unsigned int height;
  unsigned int max_layers;
  unsigned int max_bitrate[cricket::SBM_COUNT];
  unsigned int target_bitrate[cricket::SBM_COUNT];
};

static const SimulcastFormat kSimulcastFormats[] = {
  {1280, 720, 3,
      {kSimStream4Bitrate, kSimStream4MaxBitrateBoosted,
       kSimStream4MaxBitrateXd},
      {kSimStream4Bitrate, kSimStream4TargetBitrateBoosted,
       kSimStream4TargetBitrateXd}},
  {960, 540, 3,
      {kSimStream3Bitrate, kSimStream3MaxBitrateBoosted,
       kSimStream3MaxBitrateXd},
      {kSimStream3Bitrate, kSimStream3TargetBitrateBoosted,
       kSimStream3TargetBitrateXd}},
  {640, 360, 2,
      {kSimStream2Bitrate, kSimStream2MaxBitrateBoosted,
       kSimStream2MaxBitrateXd},
      {kSimStream2Bitrate, kSimStream2TargetBitrateBoosted,
       kSimStream2TargetBitrateXd}},
  {480, 270, 2,
      {kSimStream1Bitrate, kSimStream1MaxBitrateBoosted,
       kSimStream1MaxBitrateXd},
      {kSimStream1Bitrate, kSimStream1TargetBitrateBoosted,
       kSimStream1TargetBitrateXd}},
  {320, 180, 1,
      {kSimStream0Bitrate, kSimStream0MaxBitrateBoosted,
       kSimStream0MaxBitrateXd},
      {kSimStream0Bitrate, kSimStream0TargetBitrateBoosted,
       kSimStream0TargetBitrateXd}},
  {0, 0, 1,
      {kSimStream0Bitrate, kSimStream0MaxBitrateBoosted,
       kSimStream0MaxBitrateXd},
      {kSimStream0Bitrate, kSimStream0TargetBitrateBoosted,
       kSimStream0TargetBitrateXd}}
};

// Test fixture to test WebRtcVideoEngine with a fake webrtc::VideoEngine.
// Useful for testing failure paths.
class WebRtcVideoEngineSimulcastTestFake : public testing::Test,
  public sigslot::has_slots<> {
 public:
  WebRtcVideoEngineSimulcastTestFake()
      : vie_(kVideoCodecs, ARRAY_SIZE(kVideoCodecs)),
        cpu_monitor_(new rtc::FakeCpuMonitor(
            rtc::Thread::Current())),
        engine_(NULL,  // cricket::WebRtcVoiceEngineExtended
                new FakeViEWrapper(&vie_), cpu_monitor_),
        channel_(NULL),
        voice_channel_(NULL),
        last_error_(cricket::VideoMediaChannel::ERROR_NONE) {
  }
  bool SetupEngine() {
    bool result = engine_.Init(rtc::Thread::Current());
    if (result) {
      channel_ = engine_.CreateChannel(cricket::VideoOptions(), voice_channel_);
      channel_->SignalMediaError.connect(this,
          &WebRtcVideoEngineSimulcastTestFake::OnMediaError);
      result = (channel_ != NULL);
    }
    return result;
  }
  void OnMediaError(uint32 ssrc, cricket::VideoMediaChannel::Error error) {
    last_error_ = error;
  }
  bool SendI420Frame(int width, int height) {
    cricket::FakeVideoCapturer capturer;
    return SendI420Frame(&capturer, width, height);
  }

  bool SendI420Frame(
      cricket::FakeVideoCapturer* capturer, int width, int height) {
    if (NULL == channel_) {
      return false;
    }
    cricket::WebRtcVideoFrame frame;
    if (!frame.InitToBlack(width, height, 1, 1, 0, 0)) {
      return false;
    }
    channel_->SendFrame(capturer, &frame);
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
  void VerifyVP8SendCodec(
      int channel_num,
      unsigned int width,
      unsigned int height,
      unsigned int layers = 0,
      unsigned int max_bitrate = kMaxBandwidthKbps,
      unsigned int min_bitrate = kMinBandwidthKbps,
      unsigned int start_bitrate = kStartBandwidthKbps,
      unsigned int fps = 30,
      unsigned int max_quantization = 0,
      cricket::SimulcastBitrateMode bitrate_mode = cricket::SBM_NORMAL) {
    webrtc::VideoCodec gcodec;
    EXPECT_EQ(0, vie_.GetSendCodec(channel_num, gcodec));

    // Video codec properties.
    EXPECT_EQ(webrtc::kVideoCodecVP8, gcodec.codecType);
    EXPECT_STREQ("VP8", gcodec.plName);
    EXPECT_EQ(100, gcodec.plType);
    EXPECT_EQ(width, gcodec.width);
    EXPECT_EQ(height, gcodec.height);
    EXPECT_EQ(std::min(start_bitrate, max_bitrate), gcodec.startBitrate);
    EXPECT_EQ(max_bitrate, gcodec.maxBitrate);
    EXPECT_EQ(min_bitrate, gcodec.minBitrate);
    EXPECT_EQ(fps, gcodec.maxFramerate);
    // VP8 specific.
    EXPECT_FALSE(gcodec.codecSpecific.VP8.pictureLossIndicationOn);
    EXPECT_FALSE(gcodec.codecSpecific.VP8.feedbackModeOn);
    EXPECT_EQ(webrtc::kComplexityNormal, gcodec.codecSpecific.VP8.complexity);
    EXPECT_EQ(webrtc::kResilienceOff, gcodec.codecSpecific.VP8.resilience);
    EXPECT_EQ(max_quantization, gcodec.qpMax);
    // Simulcast.
    EXPECT_EQ(layers, gcodec.numberOfSimulcastStreams);
    if (layers > 0) {
      EXPECT_EQ(kSimulcastNumberOfTemporalLayers,
                gcodec.codecSpecific.VP8.numberOfTemporalLayers);
    } else {
      EXPECT_EQ(kNumberOfTemporalLayers,
                gcodec.codecSpecific.VP8.numberOfTemporalLayers);
    }
    for (int i = 0; i < static_cast<int>(layers) - 1; ++i) {
      EXPECT_EQ(gcodec.width / (0x01 << (layers - 1 - i)),
                gcodec.simulcastStream[i].width);
      EXPECT_EQ(gcodec.height / (0x01 << (layers - 1 - i)),
                gcodec.simulcastStream[i].height);
      EXPECT_EQ(kSimulcastNumberOfTemporalLayers,
                gcodec.simulcastStream[i].numberOfTemporalLayers);
      EXPECT_EQ(FindSimulcastMaxBitrate(gcodec.simulcastStream[i].width,
                                        gcodec.simulcastStream[i].height,
                                        bitrate_mode),
                gcodec.simulcastStream[i].maxBitrate);
      EXPECT_EQ(gcodec.qpMax,
                gcodec.simulcastStream[i].qpMax);
    }
    if (layers > 0)
      EXPECT_EQ(kMinBandwidthKbps, gcodec.simulcastStream[0].minBitrate);
  }
  unsigned int FindSimulcastMaxBitrate(
      unsigned int width,
      unsigned int height,
      cricket::SimulcastBitrateMode bitrate_mode) {
    for (size_t i = 0; i < ARRAY_SIZE(kSimulcastFormats); ++i) {
      if (width >= kSimulcastFormats[i].width &&
          height >= kSimulcastFormats[i].height) {
        return kSimulcastFormats[i].max_bitrate[bitrate_mode];
      }
    }
    return 0;
  }
  void SetUp2SimulcastStreams(
      cricket::VideoOptions::HighestBitrate high_bitrate_mode,
      const cricket::VideoCodec& codec) {
    EXPECT_TRUE(SetupEngine());
    cricket::VideoOptions options;
    options.conference_mode.Set(true);
    options.video_highest_bitrate.Set(high_bitrate_mode);
    EXPECT_TRUE(channel_->SetOptions(options));
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs2))));
    std::vector<cricket::VideoCodec> codec_list;
    codec_list.push_back(codec);
    SendI420Frame(kVP8Codec.width, kVP8Codec.height);
    EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  }
  void SetUp3SimulcastStreams(
      cricket::VideoOptions::HighestBitrate high_bitrate_mode,
      const cricket::VideoCodec& codec) {
    EXPECT_TRUE(SetupEngine());
    cricket::VideoOptions options;
    options.conference_mode.Set(true);
    options.video_highest_bitrate.Set(high_bitrate_mode);
    EXPECT_TRUE(channel_->SetOptions(options));

    // Set max settings of 1280x720x30 to make sure we can have maximumly 3
    // simulcast streams.
    EXPECT_TRUE(engine_.SetDefaultEncoderConfig(
        cricket::VideoEncoderConfig(kVP8Codec720p)));

    EXPECT_TRUE(channel_->AddSendStream(
        cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs3))));
    std::vector<cricket::VideoCodec> codec_list;
    codec_list.push_back(codec);
    SendI420Frame(kVP8Codec720p.width, kVP8Codec720p.height);
    EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  }
  void TestSimulcastAdapter(const cricket::VideoCodec& send_codec,
                            bool expect_use_adapter) {
    // Setup
    engine_.SetExternalEncoderFactory(&encoder_factory_);
    EXPECT_TRUE(SetupEngine());
    std::vector<cricket::VideoCodec> codecs;
    codecs.push_back(send_codec);
    EXPECT_TRUE(channel_->SetSendCodecs(codecs));
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kSsrc)));

    // Verify
    int channel_num = vie_.GetLastChannel();
    EXPECT_EQ(1, vie_.GetNumExternalEncoderRegistered(channel_num));
    // If simulcast adapter is used, no external encoder instance will be
    // created at this point. Otherwise 1 instance should have been created.
    int num_enc_instance = expect_use_adapter ? 0 : 1;
    EXPECT_EQ(num_enc_instance, encoder_factory_.GetNumCreatedEncoders());

    // Clean up
    EXPECT_TRUE(channel_->RemoveSendStream(kSsrc));
  }
  virtual void TearDown() {
    delete channel_;
    engine_.Terminate();
  }

 protected:
  cricket::FakeWebRtcVideoEngine vie_;
  cricket::FakeWebRtcVideoDecoderFactory decoder_factory_;
  cricket::FakeWebRtcVideoEncoderFactory encoder_factory_;
  rtc::FakeCpuMonitor* cpu_monitor_;
  cricket::WebRtcVideoEngine engine_;
  cricket::WebRtcVideoMediaChannel* channel_;
  cricket::WebRtcVoiceMediaChannel* voice_channel_;
  cricket::VideoMediaChannel::Error last_error_;
};

// Test fixtures to test WebRtcVideoEngine with a real
// webrtc::VideoEngine.
class WebRtcVideoEngineSimulcastTest
    : public VideoEngineTest<cricket::WebRtcVideoEngine> {
 protected:
  typedef VideoEngineTest<cricket::WebRtcVideoEngine> Base;
};
class WebRtcVideoMediaChannelSimulcastTest
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
  // Tests that simulcast sends rtp packets containing expected number of
  // layers and payload type.
  void SimulcastSend(const cricket::VideoCodec& codec,
                     const std::vector<uint32>& ssrcs) {
    // Remove stream added in Setup.
    EXPECT_TRUE(channel_->RemoveSendStream(kSsrc));

    // Setup channel for sending and receiving simulcast packets.
    cricket::VideoOptions vmo;
    vmo.conference_mode.Set(true);
    EXPECT_TRUE(channel_->SetOptions(vmo));
    EXPECT_TRUE(SetOneCodec(codec));
    if (ssrcs.size() > 1) {
      cricket::StreamParams sp;
      cricket::SsrcGroup sg(cricket::kSimSsrcGroupSemantics, ssrcs);
      sp.ssrcs = ssrcs;
      sp.ssrc_groups.push_back(sg);
      sp.cname = "cname";
      EXPECT_TRUE(channel_->AddSendStream(sp));
    } else {
      EXPECT_TRUE(channel_->AddSendStream(
          cricket::StreamParams::CreateLegacy(ssrcs[0])));
    }

    // Re-configure capturer.
    channel_->SetCapturer(kSsrc, NULL);
    cricket::VideoFormat capture_format(codec.width, codec.height,
        cricket::VideoFormat::FpsToInterval(codec.framerate),
        cricket::FOURCC_I420);
    EXPECT_TRUE(channel_->SetCapturer(ssrcs[0], video_capturer_.get()));
    EXPECT_EQ(cricket::CS_RUNNING, video_capturer_->Start(capture_format));

    EXPECT_TRUE(SetSend(true));

    // Send one frame.
    EXPECT_EQ(0, NumRtpPackets());
    EXPECT_EQ(0, NumRtpBytes());
    EXPECT_EQ(0, NumSentSsrcs());
    EXPECT_TRUE(SendFrame());

    // Verify received expected simulcast layers.
    EXPECT_TRUE_WAIT(NumSentSsrcs() == static_cast<int>(ssrcs.size()),
        10 * kTimeout);

    // Stop sending before counting the packets, as padding will keep being sent
    // which can cause a difference between total number of packets and the
    // sum of packets on each ssrc if padding is sent in the summation loop.
    EXPECT_TRUE(SetSend(false));

    int total_num_packets = 0;
    int total_num_bytes = 0;
    for (size_t i = 0; i < ssrcs.size(); ++i) {
      EXPECT_GE(NumRtpPackets(ssrcs[i]), 1);
      EXPECT_GT(NumRtpBytes(ssrcs[i]), NumRtpPackets(ssrcs[i]));
      total_num_packets += NumRtpPackets(ssrcs[i]);
      total_num_bytes += NumRtpBytes(ssrcs[i]);
    }
    EXPECT_EQ(total_num_packets, NumRtpPackets());
    EXPECT_EQ(total_num_bytes, NumRtpBytes());

    // All packets have the given payload type.
    for (int i = 0; i < total_num_packets; ++i) {
      rtc::scoped_ptr<const rtc::Buffer> p(GetRtpPacket(i));
      EXPECT_EQ(codec.id, GetPayloadType(p.get()));
    }
  }
  void TestScreencastSettings() {
    // Remove stream added in SetUp.
    EXPECT_TRUE(channel_->RemoveSendStream(kSsrc));

    cricket::VideoOptions options;
    options.conference_mode.Set(true);
    // Enable noise reduction to ensure that it's being disabled for
    // screencasting.
    options.video_noise_reduction.Set(true);
    EXPECT_TRUE(channel_->SetOptions(options));

    // Setup a simulcast stream and verify that we have the correct
    // settings.
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs2))));
    cricket::VideoCodec codec(DefaultCodec());
    EXPECT_TRUE(SetOneCodec(codec));

    channel_->SetCapturer(kSsrc, NULL);
    cricket::VideoFormat capture_format(codec.width, codec.height,
        cricket::VideoFormat::FpsToInterval(codec.framerate),
        cricket::FOURCC_I420);
    EXPECT_TRUE(channel_->SetCapturer(kSsrcs2[0], video_capturer_.get()));
    EXPECT_EQ(cricket::CS_RUNNING, video_capturer_->Start(capture_format));

    EXPECT_TRUE(SetSend(true));
    EXPECT_TRUE(SendFrame());
    webrtc::VideoCodec video_codec;
    int default_channel_id = channel_->GetDefaultChannelId();
    EXPECT_EQ(0, channel_->engine()->vie()->codec()->GetSendCodec(
        default_channel_id, video_codec));
    EXPECT_FALSE(video_codec.codecSpecific.VP8.automaticResizeOn);
    EXPECT_TRUE(video_codec.codecSpecific.VP8.denoisingOn);
    EXPECT_EQ(webrtc::kRealtimeVideo, video_codec.mode);
    EXPECT_EQ(3, video_codec.codecSpecific.VP8.numberOfTemporalLayers);
    EXPECT_EQ(2, video_codec.numberOfSimulcastStreams);
    EXPECT_NE(0u, video_codec.simulcastStream[0].minBitrate);
    EXPECT_NE(0u, video_codec.simulcastStream[0].targetBitrate);
    EXPECT_NE(0u, video_codec.simulcastStream[0].maxBitrate);
    EXPECT_NE(0u, video_codec.simulcastStream[1].minBitrate);
    EXPECT_NE(0u, video_codec.simulcastStream[1].targetBitrate);
    EXPECT_NE(0u, video_codec.simulcastStream[1].maxBitrate);

    // Register a fake screencast capturer and verify that the video settings
    // change as expected.
    rtc::scoped_ptr<cricket::FakeVideoCapturer> capturer(
        new cricket::FakeVideoCapturer);
    capturer->SetScreencast(true);
    const std::vector<cricket::VideoFormat>* formats =
        capturer->GetSupportedFormats();
    capture_format = (*formats)[0];
    EXPECT_EQ(cricket::CS_RUNNING, capturer->Start(capture_format));
    EXPECT_TRUE(channel_->SetCapturer(kSsrcs2[0], capturer.get()));
    EXPECT_TRUE(rtc::Thread::Current()->ProcessMessages(30));
    EXPECT_TRUE(capturer->CaptureFrame());
    default_channel_id = channel_->GetDefaultChannelId();
    EXPECT_EQ(0, channel_->engine()->vie()->codec()->GetSendCodec(
        default_channel_id, video_codec));
    EXPECT_FALSE(video_codec.codecSpecific.VP8.automaticResizeOn);
    EXPECT_FALSE(video_codec.codecSpecific.VP8.denoisingOn);
    EXPECT_EQ(webrtc::kScreensharing, video_codec.mode);
    const int expected_num_layers = 2;
    EXPECT_EQ(expected_num_layers,
              video_codec.codecSpecific.VP8.numberOfTemporalLayers);
    EXPECT_EQ(2, video_codec.numberOfSimulcastStreams);
    EXPECT_EQ(0u, video_codec.simulcastStream[0].minBitrate);
    EXPECT_EQ(0u, video_codec.simulcastStream[0].targetBitrate);
    EXPECT_EQ(0u, video_codec.simulcastStream[0].maxBitrate);

    // Make sure that removing screencast restores the simulcast settings.
    EXPECT_TRUE(channel_->SetCapturer(kSsrcs2[0], video_capturer_.get()));
    EXPECT_TRUE(WaitAndSendFrame(30));
    EXPECT_EQ(0, channel_->engine()->vie()->codec()->GetSendCodec(
        default_channel_id, video_codec));
    EXPECT_FALSE(video_codec.codecSpecific.VP8.automaticResizeOn);
    EXPECT_TRUE(video_codec.codecSpecific.VP8.denoisingOn);
    EXPECT_EQ(webrtc::kRealtimeVideo, video_codec.mode);
    EXPECT_EQ(3, video_codec.codecSpecific.VP8.numberOfTemporalLayers);
    EXPECT_EQ(2, video_codec.numberOfSimulcastStreams);
    EXPECT_NE(0u, video_codec.simulcastStream[0].minBitrate);
    EXPECT_NE(0u, video_codec.simulcastStream[0].targetBitrate);
    EXPECT_NE(0u, video_codec.simulcastStream[0].maxBitrate);
    EXPECT_NE(0u, video_codec.simulcastStream[1].minBitrate);
    EXPECT_NE(0u, video_codec.simulcastStream[1].targetBitrate);
    EXPECT_NE(0u, video_codec.simulcastStream[1].maxBitrate);
  }
};

// Test that we apply send codec with simulcast properly.
TEST_F(WebRtcVideoEngineSimulcastTestFake, SetSendCodecsWith1SimulcastStreams) {
  EXPECT_TRUE(SetupEngine());
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));

  // One ssrc simulcast StreamParams should be rejected.
  EXPECT_FALSE(channel_->AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs1))));
}

TEST_F(WebRtcVideoEngineSimulcastTestFake, SetSendCodecsWith2SimulcastStreams) {
  cricket::VideoCodec codec(kVP8Codec);
  SetUp2SimulcastStreams(cricket::VideoOptions::NORMAL, codec);
  int channel_num = vie_.GetLastChannel();
  VerifyVP8SendCodec(channel_num, codec.width, codec.height, 2,
                     kSimStream0Bitrate + kSimStream2MaxBitrateBoosted);
}

TEST_F(WebRtcVideoEngineSimulcastTestFake, SetSendCodecsWith3SimulcastStreams) {
  cricket::VideoCodec codec(kVP8Codec720p);
  SetUp3SimulcastStreams(cricket::VideoOptions::NORMAL, codec);
  int channel_num = vie_.GetLastChannel();
  VerifyVP8SendCodec(channel_num, codec.width, codec.height, 3,
                     kSimStream0Bitrate + kSimStream2Bitrate +
                     kSimStream4Bitrate);
}

TEST_F(WebRtcVideoEngineSimulcastTestFake,
       SetSendCodecsWith2SimulcastStreamsHighBitrateMode) {
  cricket::VideoCodec codec(kVP8Codec);
  SetUp2SimulcastStreams(cricket::VideoOptions::HIGH, codec);
  int channel_num = vie_.GetLastChannel();
  VerifyVP8SendCodec(channel_num, codec.width, codec.height, 2,
                     kSimStream0TargetBitrateBoosted +
                     kSimStream2MaxBitrateBoosted, kMinBandwidthKbps,
                     kStartBandwidthKbps, 30, 0, cricket::SBM_HIGH);
}

TEST_F(WebRtcVideoEngineSimulcastTestFake,
       SetSendCodecsWith3SimulcastStreamsHighBitrateMode) {
  cricket::VideoCodec codec(kVP8Codec720p);
  SetUp3SimulcastStreams(cricket::VideoOptions::HIGH, codec);
  int channel_num = vie_.GetLastChannel();
  VerifyVP8SendCodec(channel_num, codec.width, codec.height, 3,
                     kSimStream0TargetBitrateBoosted +
                     kSimStream2TargetBitrateBoosted +
                     kSimStream4MaxBitrateBoosted, kMinBandwidthKbps,
                     kStartBandwidthKbps, 30, 0, cricket::SBM_HIGH);
}

TEST_F(WebRtcVideoEngineSimulcastTestFake,
       SetSendCodecsWith2SimulcastStreamsVeryHighBitrateMode) {
  cricket::VideoCodec codec(kVP8Codec);
  SetUp2SimulcastStreams(cricket::VideoOptions::VERY_HIGH,
                         codec);
  int channel_num = vie_.GetLastChannel();
  VerifyVP8SendCodec(channel_num, codec.width, codec.height, 2,
                     kSimStream0TargetBitrateXd +
                     kSimStream2MaxBitrateXd, kMinBandwidthKbps,
                     kStartBandwidthKbps, 30, 0, cricket::SBM_VERY_HIGH);
}

TEST_F(WebRtcVideoEngineSimulcastTestFake,
       SetSendCodecsWith3SimulcastStreamsVeryHighBitrateMode) {
  cricket::VideoCodec codec(kVP8Codec720p);
  SetUp3SimulcastStreams(cricket::VideoOptions::VERY_HIGH,
                         codec);
  int channel_num = vie_.GetLastChannel();
  VerifyVP8SendCodec(channel_num, codec.width, codec.height, 3,
                     kSimStream0TargetBitrateXd +
                     kSimStream2TargetBitrateXd +
                     kSimStream4MaxBitrateXd, kMinBandwidthKbps,
                     kStartBandwidthKbps, 30, 0, cricket::SBM_VERY_HIGH);
}

// Test that we normalize send codec format size in simulcast.
TEST_F(WebRtcVideoEngineSimulcastTestFake,
       SetSendCodecsWithOddSizeInSimulcast) {
  EXPECT_TRUE(SetupEngine());
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  int channel_num = vie_.GetLastChannel();

  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs2))));

  // Set send codec and verify we get two simulcast sub-streams with
  // normalized size.
  cricket::VideoCodec codec(kVP8Codec270p);
  codec.width += 0x01;
  codec.height += 0x01;
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(codec);
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));

  codec.width &= ~0x01;
  codec.height &= ~0x01;
  VerifyVP8SendCodec(channel_num, codec.width, codec.height, 2,
                     kSimStream0Bitrate + kSimStream1MaxBitrateBoosted);
}

// Test that if we add a stream with RTX SSRC's, SSRC's get set correctly.
TEST_F(WebRtcVideoEngineSimulcastTestFake, TestStreamWithRtx) {
  EXPECT_TRUE(SetupEngine());
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  int channel_num = vie_.GetLastChannel();

  // Set max settings of 1280x720x30 to make sure we can have maximumly 3
  // simulcast streams.
  EXPECT_TRUE(engine_.SetDefaultEncoderConfig(
    cricket::VideoEncoderConfig(kVP8Codec720p)));

  // Verify only the first SSRC is set here.
  // The rest are set during SetSendCodecs.
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimWithRtxStreamParams("cname",
                                            MAKE_VECTOR(kSsrcs3),
                                            MAKE_VECTOR(kRtxSsrcs3))));
  EXPECT_EQ(1, vie_.GetNumSsrcs(channel_num));
  EXPECT_EQ(1, vie_.GetNumRtxSsrcs(channel_num));

  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(kVP8Codec720p);
  cricket::VideoCodec rtx_codec(96, "rtx", 0, 0, 0, 0);
  rtx_codec.SetParam("apt", kVP8Codec.id);
  codec_list.push_back(rtx_codec);
  SendI420Frame(kVP8Codec720p.width, kVP8Codec720p.height);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));

  // RTX payload type should now be set.
  EXPECT_EQ(96, vie_.GetRtxSendPayloadType(channel_num));

  // Verify all SSRCs are set after SetSendCodecs.
  EXPECT_EQ(3, vie_.GetNumSsrcs(channel_num));
  EXPECT_EQ(3, vie_.GetNumRtxSsrcs(channel_num));
  EXPECT_EQ(1, vie_.GetSsrc(channel_num, 0));
  EXPECT_EQ(2, vie_.GetSsrc(channel_num, 1));
  EXPECT_EQ(3, vie_.GetSsrc(channel_num, 2));
  EXPECT_EQ(4, vie_.GetRtxSsrc(channel_num, 0));
  EXPECT_EQ(5, vie_.GetRtxSsrc(channel_num, 1));
  EXPECT_EQ(6, vie_.GetRtxSsrc(channel_num, 2));
  uint32 ssrc;
  EXPECT_EQ(0, vie_.GetLocalSSRC(channel_num, ssrc));
  EXPECT_EQ(1U, ssrc);
}

// Test that if we get too few ssrcs are given in AddSendStream(),
// only supported sub-streams will be added.
TEST_F(WebRtcVideoEngineSimulcastTestFake, TooFewSimulcastSsrcs) {
  EXPECT_TRUE(SetupEngine());
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  int channel_num = vie_.GetLastChannel();

  // Set max settings of 1280x720x30 to make sure we can have maximumly 3
  // simulcast streams.
  EXPECT_TRUE(engine_.SetDefaultEncoderConfig(
    cricket::VideoEncoderConfig(kVP8Codec720p)));

  // Verify only the first SSRC is set here.
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs2))));
  EXPECT_EQ(1, vie_.GetNumSsrcs(channel_num));

  cricket::VideoCodec codec(kVP8Codec720p);
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(codec);
  SendI420Frame(kVP8Codec720p.width, kVP8Codec720p.height);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));

  // Verify all SSRCs are set after SetSendCodecs.
  EXPECT_EQ(2, vie_.GetNumSsrcs(channel_num));
  EXPECT_EQ(1, vie_.GetSsrc(channel_num, 0));
  EXPECT_EQ(2, vie_.GetSsrc(channel_num, 1));
  uint32 ssrc;
  EXPECT_EQ(0, vie_.GetLocalSSRC(channel_num, ssrc));
  EXPECT_EQ(1U, ssrc);
  VerifyVP8SendCodec(channel_num, 640, 360, 2,
      kSimStream0Bitrate + kSimStream2MaxBitrateBoosted);
}

// Test that even more than enough ssrcs are given in AddSendStream(),
// only supported sub-streams will be added.
TEST_F(WebRtcVideoEngineSimulcastTestFake, MoreThanEnoughSimulcastSscrs) {
  EXPECT_TRUE(SetupEngine());
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  int channel_num = vie_.GetLastChannel();

  // Set max settings of 1280x720x30 to make sure we can have maximumly 3
  // simulcast streams.
  EXPECT_TRUE(engine_.SetDefaultEncoderConfig(
    cricket::VideoEncoderConfig(kVP8Codec720p)));

  // Verify only the first SSRC is set here.
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs4))));
  EXPECT_EQ(1, vie_.GetNumSsrcs(channel_num));

  cricket::VideoCodec codec(kVP8Codec720p);
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(codec);
  SendI420Frame(kVP8Codec720p.width, kVP8Codec720p.height);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));

  // Verify all SSRCs are set after SetSendCodecs.
  EXPECT_EQ(3, vie_.GetNumSsrcs(channel_num));
  EXPECT_EQ(1, vie_.GetSsrc(channel_num, 0));
  EXPECT_EQ(2, vie_.GetSsrc(channel_num, 1));
  EXPECT_EQ(3, vie_.GetSsrc(channel_num, 2));
  uint32 ssrc;
  EXPECT_EQ(0, vie_.GetLocalSSRC(channel_num, ssrc));
  EXPECT_EQ(1U, ssrc);
  VerifyVP8SendCodec(channel_num, codec.width, codec.height, 3,
      kSimStream0Bitrate + kSimStream2Bitrate + kSimStream4Bitrate);
}

// Test that SetSendStreamFormat works well with simulcast.
TEST_F(WebRtcVideoEngineSimulcastTestFake, SetSendStreamFormatWithSimulcast) {
  EXPECT_TRUE(SetupEngine());
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  int channel_num = vie_.GetLastChannel();

  // Verify SetSendStreamFormat fail before there is a send stream.
  cricket::VideoFormat format = engine_.default_codec_format();
  EXPECT_FALSE(channel_->SetSendStreamFormat(1, format));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs2))));

  // Capture format HD
  cricket::FakeVideoCapturer video_capturer;
  const std::vector<cricket::VideoFormat>* formats =
      video_capturer.GetSupportedFormats();
  cricket::VideoFormat capture_format_hd = (*formats)[0];
  EXPECT_EQ(cricket::CS_RUNNING, video_capturer.Start(capture_format_hd));
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs2[0], &video_capturer));

  cricket::VideoCodec codec(kVP8Codec360p);
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(codec);
  SendI420Frame(&video_capturer, kVP8Codec.width, kVP8Codec.height);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));

  EXPECT_EQ(2, vie_.GetNumSsrcs(channel_num));
  EXPECT_EQ(1, vie_.GetSsrc(channel_num, 0));
  EXPECT_EQ(2, vie_.GetSsrc(channel_num, 1));
  VerifyVP8SendCodec(channel_num, codec.width, codec.height, 2,
                     kSimStream0Bitrate + kSimStream2MaxBitrateBoosted);

  // Verify SetSendStreamFormat with the same resolution doesn't
  // change the number of simulcast sub-streams.
  format.width = codec.width;
  format.height = codec.height;
  EXPECT_TRUE(channel_->SetSendStreamFormat(1, format));
  EXPECT_EQ(2, vie_.GetNumSsrcs(channel_num));
  EXPECT_EQ(1, vie_.GetSsrc(channel_num, 0));
  EXPECT_EQ(2, vie_.GetSsrc(channel_num, 1));

  // Capture format HD -> adapt (OnOutputFormatRequest VGA) -> VGA.
  EXPECT_TRUE(video_capturer.CaptureFrame());
  VerifyVP8SendCodec(channel_num, codec.width, codec.height, 2,
                     kSimStream0Bitrate + kSimStream2MaxBitrateBoosted);

  // Now, ask for a small resolution and verify that we only get one stream.
  format.width = codec.width / 2;
  format.height = codec.height / 2;
  EXPECT_TRUE(channel_->SetSendStreamFormat(1, format));
  // Capture format HD -> adapt (OnOutputFormatRequest QVGA) -> QVGA.
  EXPECT_TRUE(video_capturer.CaptureFrame());
  VerifyVP8SendCodec(channel_num, format.width, format.height, 1,
                     kSimStream0MaxBitrateBoosted);

  // Bump resolution back and verify there are 2 sub-streams again.
  format.width = codec.width;
  format.height = codec.height;
  EXPECT_TRUE(channel_->SetSendStreamFormat(1, format));
  EXPECT_EQ(2, vie_.GetNumSsrcs(channel_num));
  EXPECT_EQ(1, vie_.GetSsrc(channel_num, 0));
  EXPECT_EQ(2, vie_.GetSsrc(channel_num, 1));
  // Capture format HD -> adapt (OnOutputFormatRequest VGA) -> VGA.
  EXPECT_TRUE(video_capturer.CaptureFrame());
  VerifyVP8SendCodec(channel_num, format.width, format.height, 2,
                     kSimStream0Bitrate + kSimStream2MaxBitrateBoosted);

  // Verify SetSendStreamFormat can use either ssrc in the simulcast
  // ssrc group.
  format.width = codec.width / 2;
  format.height = codec.height / 2;
  EXPECT_TRUE(channel_->SetSendStreamFormat(2, format));
  EXPECT_EQ(2, vie_.GetNumSsrcs(channel_num));
  EXPECT_EQ(1, vie_.GetSsrc(channel_num, 0));
  EXPECT_EQ(2, vie_.GetSsrc(channel_num, 1));
  // Capture format HD -> adapt (OnOutputFormatRequest QVGA) -> QVGA.
  EXPECT_TRUE(video_capturer.CaptureFrame());
  VerifyVP8SendCodec(channel_num, format.width, format.height, 1,
                     kSimStream0MaxBitrateBoosted);

  // Verify SetSendStreamFormat with bad ssrc fail.
  format.width = codec.width;
  format.height = codec.height;
  EXPECT_FALSE(channel_->SetSendStreamFormat(3, format));
  EXPECT_EQ(2, vie_.GetNumSsrcs(channel_num));
  EXPECT_EQ(1, vie_.GetSsrc(channel_num, 0));
  EXPECT_EQ(2, vie_.GetSsrc(channel_num, 1));
  VerifyVP8SendCodec(channel_num, format.width / 2, format.height / 2, 1,
                     kSimStream0MaxBitrateBoosted);
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs2[0], NULL));
}

// Test that simulcast send codec is reset on new video frame size.
TEST_F(WebRtcVideoEngineSimulcastTestFake,
       ResetSimulcastSendCodecOnNewFrameSize) {
  EXPECT_TRUE(SetupEngine());
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  int channel_num = vie_.GetLastChannel();

  // Set send codec.
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs2))));
  cricket::VideoCodec codec(kVP8Codec);
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_EQ(1, vie_.GetNumSetSendCodecs());

  // Feed in frames with different sizes and verify simulcast send codec
  // changes accordingly.

  // Send an odd frame. Verify format size get normalized.
  SendI420Frame(kVP8Codec.width | 0x01, kVP8Codec.height | 0x01);
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height, 2,
                     kSimStream0Bitrate + kSimStream2MaxBitrateBoosted);
  EXPECT_EQ(2, vie_.GetNumSetSendCodecs());

  // Send a big frame. Verify format size never exceed original send codec
  // format size.
  SendI420Frame(kVP8Codec.width * 2, kVP8Codec.height * 2);
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height, 2,
                     kSimStream0Bitrate + kSimStream2MaxBitrateBoosted);
  EXPECT_EQ(2, vie_.GetNumSetSendCodecs());

  // Send a small frame. Verify format size is bumped in order to meet
  // the minimum simulcast reqirement while keeping the aspect ratio.
  SendI420Frame(kVP8Codec.width / 2, kVP8Codec.height / 2);
  // Need at least 480x270 to have simulcast.
  VerifyVP8SendCodec(channel_num, kVP8Codec.width / 2, kVP8Codec.height / 2, 1,
                     kSimStream0MaxBitrateBoosted);
  EXPECT_EQ(3, vie_.GetNumSetSendCodecs());

  // Send a normal frame. Verify format size is back to normal.
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height, 2,
                     kSimStream0Bitrate + kSimStream2MaxBitrateBoosted);
  EXPECT_EQ(4, vie_.GetNumSetSendCodecs());

  // Send an odd frame again.
  SendI420Frame(kVP8Codec.width | 0x01, kVP8Codec.height | 0x01);
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height, 2,
                     kSimStream0Bitrate + kSimStream2MaxBitrateBoosted);
  EXPECT_EQ(4, vie_.GetNumSetSendCodecs());
}

// Test that simulcast send codec is reset on new portait mode video frame.
TEST_F(WebRtcVideoEngineSimulcastTestFake,
       ResetSimulcastSendCodecOnNewPortaitFrame) {
  EXPECT_TRUE(SetupEngine());
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  int channel_num = vie_.GetLastChannel();

  // Set send codec.
  EXPECT_TRUE(engine_.SetDefaultEncoderConfig(
      cricket::VideoEncoderConfig(kVP8Codec720p)));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs2))));
  cricket::VideoCodec codec(kVP8Codec720p);
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_EQ(1, vie_.GetNumSetSendCodecs());

  // Feed in frames with different sizes and verify simulcast send codec
  // changes accordingly.

  // Send a portait mode frame.
  SendI420Frame(kVP8Codec.height, kVP8Codec.width);
  VerifyVP8SendCodec(channel_num, kVP8Codec.height, kVP8Codec.width, 2,
                     kSimStream0Bitrate + kSimStream2MaxBitrateBoosted);
  EXPECT_EQ(2, vie_.GetNumSetSendCodecs());
}

TEST_F(WebRtcVideoEngineSimulcastTestFake,
       SetBandwidthInConferenceWithSimulcast) {
  cricket::VideoCodec ccodec(kVP8Codec720p);
  SetUp3SimulcastStreams(cricket::VideoOptions::NORMAL, ccodec);

  // A higher value than the sum of max of all layers should add the extra
  // bandwidth to the higher layer.
  uint32 max_bandwidth = 5000;
  EXPECT_TRUE(channel_->SetMaxSendBandwidth(max_bandwidth * 1000));

  int channel_num = vie_.GetLastChannel();
  webrtc::VideoCodec codec;
  codec.width = 0; codec.height = 0;
  EXPECT_EQ(0, vie_.GetSendCodec(channel_num, codec));
  EXPECT_EQ(max_bandwidth, codec.maxBitrate);

  int nr_layers = 3;
  uint32 sum_bandwidth = 0;
  for (int i = 0; i < nr_layers; ++i) {
    if (i == nr_layers - 1) {
      EXPECT_EQ(max_bandwidth - sum_bandwidth,
                codec.simulcastStream[i].maxBitrate);
    } else {
      EXPECT_EQ(FindSimulcastMaxBitrate(codec.simulcastStream[i].width,
                                        codec.simulcastStream[i].height,
                                        cricket::SBM_NORMAL),
                codec.simulcastStream[i].maxBitrate);
      sum_bandwidth += codec.simulcastStream[i].maxBitrate;
    }
  }

  // Test setting max bandwidth below the sum of layer specific max bandwidth.
  // Basically, the simulcast max overrides the SetMaxSendBandwidth value.
  EXPECT_TRUE(channel_->SetMaxSendBandwidth(1));
  VerifyVP8SendCodec(channel_num, codec.width, codec.height, 3,
                     kSimStream0Bitrate + kSimStream2Bitrate +
                     kSimStream4Bitrate);
}

// Test that sending screencast frames in conference mode changes
// bitrate.
TEST_F(WebRtcVideoEngineSimulcastTestFake, SetBandwidthScreencastInConference) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  // Set send codec.
  cricket::VideoCodec codec(kVP8Codec);
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(codec);
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(123)));
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  EXPECT_TRUE(channel_->SetMaxSendBandwidth(1200000));
  EXPECT_EQ(2, vie_.GetNumSetSendCodecs());

  // Set conference mode and verify that this caps maxBitrate.
  webrtc::VideoCodec gcodec;
  EXPECT_EQ(0, vie_.GetSendCodec(channel_num, gcodec));
  EXPECT_GT(gcodec.maxBitrate, 1000u);
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  EXPECT_TRUE(channel_->SetSend(true));
  SendI420ScreencastFrame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_EQ(3, vie_.GetNumSetSendCodecs());
  EXPECT_EQ(0, vie_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(1000u, gcodec.maxBitrate);
}

// Test AddSendStream with simulcast rejects bad StreamParams.
TEST_F(WebRtcVideoEngineSimulcastTestFake, AddSendStreamWithBadStreamParams) {
  EXPECT_TRUE(SetupEngine());
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));

  // Verify non-sim StreamParams is rejected.
  cricket::StreamParams sp_bad;
  std::vector<uint32> ssrcs;
  ssrcs.push_back(1234);
  ssrcs.push_back(5678);
  cricket::SsrcGroup sg("bad", ssrcs);
  sp_bad.ssrcs = ssrcs;
  sp_bad.ssrc_groups.push_back(sg);
  sp_bad.cname = "cname";
  EXPECT_FALSE(channel_->AddSendStream(sp_bad));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs2))));
}

// Test AddSendStream with simulcast sets ssrc and cname correctly.
TEST_F(WebRtcVideoEngineSimulcastTestFake, AddSendStreamWithSimulcast) {
  EXPECT_TRUE(SetupEngine());
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  int channel_num = vie_.GetLastChannel();

  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs2))));

  unsigned int ssrc = 0;
  EXPECT_EQ(1, vie_.GetNumSsrcs(channel_num));
  EXPECT_EQ(0, vie_.GetLocalSSRC(channel_num, ssrc));
  EXPECT_EQ(1U, ssrc);

  char rtcp_cname[256];
  EXPECT_EQ(0, vie_.GetRTCPCName(channel_num, rtcp_cname));
  EXPECT_STREQ("cname", rtcp_cname);

  EXPECT_TRUE(channel_->RemoveSendStream(1));
}

// Test RemoveSendStream with simulcast.
TEST_F(WebRtcVideoEngineSimulcastTestFake, RemoveSendStreamWithSimulcast) {
  EXPECT_TRUE(SetupEngine());
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));

  // RemoveSendStream should fail when there is no send stream.
  EXPECT_FALSE(channel_->RemoveSendStream(1));

  cricket::StreamParams stream =
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs2));
  EXPECT_TRUE(channel_->AddSendStream(stream));

  // RemoveSendStream should fail with bad ssrc.
  EXPECT_FALSE(channel_->RemoveSendStream(3));
  EXPECT_TRUE(channel_->RemoveSendStream(1));

  // RemoveSendStream should work with either ssrc.
  EXPECT_TRUE(channel_->AddSendStream(stream));
  EXPECT_TRUE(channel_->RemoveSendStream(1));
  EXPECT_TRUE(channel_->AddSendStream(stream));
  EXPECT_TRUE(channel_->RemoveSendStream(2));

  // RemoveSendStream should fail when there is no send stream.
  EXPECT_FALSE(channel_->RemoveSendStream(1));
}

// Test AddSendStream after send codec has already been set will reset
// send codec with simulcast settings.
TEST_F(WebRtcVideoEngineSimulcastTestFake,
       AddSimulcastStreamAfterSetSendCodec) {
  EXPECT_TRUE(SetupEngine());
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  int channel_num = vie_.GetLastChannel();

  // Set send codec.
  cricket::VideoCodec codec(kVP8Codec);
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(codec);
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  VerifyVP8SendCodec(channel_num, codec.width, codec.height);

  // Add simulcast send stream and verify send codec has been reset.
  // Provide 3 ssrcs here and only 2 layers should be added.
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs3))));
  VerifyVP8SendCodec(channel_num, codec.width, codec.height, 2,
                     kSimStream0Bitrate + kSimStream2MaxBitrateBoosted);

  // Verify local SSRCs are now populated correctly.
  EXPECT_EQ(2, vie_.GetNumSsrcs(channel_num));
  EXPECT_EQ(1, vie_.GetSsrc(channel_num, 0));
  EXPECT_EQ(2, vie_.GetSsrc(channel_num, 1));
  uint32 ssrc;
  EXPECT_EQ(0, vie_.GetLocalSSRC(channel_num, ssrc));
  EXPECT_EQ(1U, ssrc);

  // Remove the stream and reset send codec with a smaller size.
  EXPECT_TRUE(channel_->RemoveSendStream(1));
  VerifyVP8SendCodec(channel_num, codec.width, codec.height, 2,
                     kSimStream0Bitrate + kSimStream2MaxBitrateBoosted);
  codec_list.clear();
  codec.width /= 2;
  codec.height /= 2;
  codec_list.push_back(codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  VerifyVP8SendCodec(channel_num, codec.width, codec.height);

  // Add simulcast send stream again and verify simulcast have one stream.
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs2))));
  VerifyVP8SendCodec(channel_num, codec.width, codec.height, 1,
                     kSimStream0MaxBitrateBoosted);

  // Verify we still have 2 SSRCs.
  EXPECT_EQ(2, vie_.GetNumSsrcs(channel_num));
  EXPECT_EQ(1, vie_.GetSsrc(channel_num, 0));
  EXPECT_EQ(2, vie_.GetSsrc(channel_num, 1));
  EXPECT_EQ(0, vie_.GetLocalSSRC(channel_num, ssrc));
  EXPECT_EQ(1U, ssrc);
}

TEST_F(WebRtcVideoEngineSimulcastTestFake, GetStatsWithMultipleSsrcs) {
  EXPECT_TRUE(SetupEngine());
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));

  // Add simulcast stream.
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs2))));

  // Set send codec and verify we get two simulcast sub-streams.
  cricket::VideoCodec codec(kVP8Codec);
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  EXPECT_TRUE(channel_->SetSend(true));

  cricket::FakeVideoCapturer video_capturer;
  const std::vector<cricket::VideoFormat>* formats =
      video_capturer.GetSupportedFormats();
  cricket::VideoFormat capture_format_vga = (*formats)[1];
  EXPECT_EQ(cricket::CS_RUNNING, video_capturer.Start(capture_format_vga));
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs2[0], &video_capturer));

  // Only require one cpu sample for testing whether the adapter is hooked up.
  cricket::CoordinatedVideoAdapter* video_adapter = NULL;
  ASSERT_TRUE(channel_->GetVideoAdapter(0u, &video_adapter));
  video_adapter->set_cpu_load_min_samples(1);
  cpu_monitor_->SignalUpdate(1, 1, 0.1f, 0.85f);

  // Capture format VGA -> adapt (OnCpuLoadUpdate downgrade) -> VGA/2.
  EXPECT_TRUE(video_capturer.CaptureFrame());

  // Get stats and verify there are 2 ssrcs.
  cricket::VideoMediaInfo info;
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.senders.size());
  ASSERT_EQ(2U, info.senders[0].ssrcs().size());
  EXPECT_EQ(1U, info.senders[0].ssrcs()[0]);
  EXPECT_EQ(2U, info.senders[0].ssrcs()[1]);
  // Verify the input/send width/height.
  EXPECT_EQ(capture_format_vga.width, info.senders[0].input_frame_width);
  EXPECT_EQ(capture_format_vga.height, info.senders[0].input_frame_height);
  EXPECT_EQ(capture_format_vga.width * 3 / 4,
            info.senders[0].send_frame_width);
  EXPECT_EQ(capture_format_vga.height * 3 / 4,
            info.senders[0].send_frame_height);

  EXPECT_TRUE(channel_->SetCapturer(kSsrcs2[0], NULL));
}

// Test receiving channel(s) local ssrc is set to the same as the first
// simulcast sending ssrc.
TEST_F(WebRtcVideoEngineSimulcastTestFake,
       AddSimulcastStreamAfterCreatingRecvChannels) {
  EXPECT_TRUE(SetupEngine());
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  int channel_num = vie_.GetLastChannel();

  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(10)));
  int receive_channel_1 = vie_.GetLastChannel();
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(11)));
  int receive_channel_2 = vie_.GetLastChannel();

  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs2))));

  unsigned int ssrc = 0;
  EXPECT_EQ(1, vie_.GetNumSsrcs(channel_num));
  EXPECT_EQ(0, vie_.GetLocalSSRC(channel_num, ssrc));
  EXPECT_EQ(1U, ssrc);

  EXPECT_EQ(0, vie_.GetLocalSSRC(receive_channel_1, ssrc));
  EXPECT_EQ(1U, ssrc);
  EXPECT_EQ(1, vie_.GetNumSsrcs(receive_channel_1));

  EXPECT_EQ(0, vie_.GetLocalSSRC(receive_channel_2, ssrc));
  EXPECT_EQ(1U, ssrc);
  EXPECT_EQ(1, vie_.GetNumSsrcs(receive_channel_2));
}

// Test 1:1 call never turn on simulcast.
TEST_F(WebRtcVideoEngineSimulcastTestFake, NoSimulcastWith1on1) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs2))));

  // Set send codec and verify there is no simulcast sub-streams.
  cricket::VideoCodec codec(kVP8Codec);
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(codec);
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  VerifyVP8SendCodec(channel_num, codec.width, codec.height);
  EXPECT_EQ(1, vie_.GetNumSsrcs(channel_num));

  // Set options with OPT_CONFERENCE flag and set send codec again.
  // Verify simulcast is used now.
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  VerifyVP8SendCodec(channel_num, codec.width, codec.height, 2,
                     kSimStream0Bitrate + kSimStream2MaxBitrateBoosted);
  EXPECT_EQ(2, vie_.GetNumSsrcs(channel_num));
  EXPECT_EQ(1, vie_.GetSsrc(channel_num, 0));
  EXPECT_EQ(2, vie_.GetSsrc(channel_num, 1));

  // Remove OPT_CONFERENCE flag and set send codec again.
  // Verify simulcast is off again.
  options.conference_mode.Set(false);
  EXPECT_TRUE(channel_->SetOptions(options));
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  VerifyVP8SendCodec(channel_num, codec.width, codec.height);

  // Verify we still have 2 SSRCs.
  EXPECT_EQ(2, vie_.GetNumSsrcs(channel_num));
  EXPECT_EQ(1, vie_.GetSsrc(channel_num, 0));
  EXPECT_EQ(2, vie_.GetSsrc(channel_num, 1));
}

// Test SetOptions with OPT_CONFERENCE flag.
TEST_F(WebRtcVideoEngineSimulcastTestFake, SetOptionsWithConferenceMode) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = vie_.GetLastChannel();

  // Set send codec.
  cricket::VideoCodec codec(kVP8Codec);
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(codec);
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));

  // Verify default send codec and bitrate.
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height);

  // Set options with OPT_CONFERENCE flag.
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));

  // Change the max bitrate and verify it has changed.
  channel_->SetMaxSendBandwidth(kSimStream2MaxBitrateBoosted * 1000);
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height, 0,
                     kSimStream2MaxBitrateBoosted);

  // Turn off conference mode and verify the max bandwidth changed
  // back.
  options.conference_mode.Set(false);
  EXPECT_TRUE(channel_->SetOptions(options));
  VerifyVP8SendCodec(channel_num, kVP8Codec.width, kVP8Codec.height);

  // Set send codec again with a smaller size.
  codec_list.clear();
  codec.width /= 2;
  codec.height /= 2;
  codec_list.push_back(codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));

  // Set options with OPT_CONFERENCE flag and set the max bitrate again.
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  channel_->SetMaxSendBandwidth(kSimStream0MaxBitrateBoosted * 1000);

  // Verify channel now has a new max bitrate.
  VerifyVP8SendCodec(channel_num, kVP8Codec.width / 2, kVP8Codec.height / 2, 0,
                     kSimStream0MaxBitrateBoosted);
}

// Test that two different streams can have different formats.
TEST_F(WebRtcVideoEngineSimulcastTestFake,
       MultipleSendStreamsDifferentFormats) {
  EXPECT_TRUE(SetupEngine());
  for (unsigned int i = 0; i < sizeof(kSsrcs2)/sizeof(kSsrcs2[0]); ++i) {
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kSsrcs2[i])));
  }
  const int channel0 = vie_.GetChannelFromLocalSsrc(kSsrcs2[0]);
  ASSERT_NE(-1, channel0);
  const int channel1 = vie_.GetChannelFromLocalSsrc(kSsrcs2[1]);
  ASSERT_NE(-1, channel1);
  ASSERT_NE(channel0, channel1);
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVP8Codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  cricket::VideoFormat format(kVP8Codec.width / 2, kVP8Codec.height / 2,
        cricket::VideoFormat::FpsToInterval(kVP8Codec.framerate / 2),
        cricket::FOURCC_I420);
  EXPECT_TRUE(channel_->SetSendStreamFormat(kSsrcs2[1], format));

  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  VerifyVP8SendCodec(channel0, kVP8Codec.width, kVP8Codec.height, 0,
                     kMaxBandwidthKbps, kMinBandwidthKbps, kStartBandwidthKbps,
                     kVP8Codec.framerate);
  VerifyVP8SendCodec(channel1, kVP8Codec.width / 2, kVP8Codec.height / 2, 0,
                     kMaxBandwidthKbps, kMinBandwidthKbps, kStartBandwidthKbps,
                     kVP8Codec.framerate / 2);
}

TEST_F(WebRtcVideoEngineSimulcastTestFake, TestAdaptToOutputFormat) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs3))));

  // Capture format HD
  cricket::FakeVideoCapturer video_capturer;
  const std::vector<cricket::VideoFormat>* formats =
      video_capturer.GetSupportedFormats();
  cricket::VideoFormat capture_format_hd = (*formats)[0];
  EXPECT_EQ(cricket::CS_RUNNING, video_capturer.Start(capture_format_hd));
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs3[0], &video_capturer));

  cricket::VideoCodec send_codec(100, "VP8", 800, 600, 30, 0);
  cricket::VideoFormat vga_format(640, 360,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420);
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(send_codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_TRUE(channel_->SetSendStreamFormat(kSsrcs3[0], vga_format));
  EXPECT_TRUE(channel_->SetSend(true));

  // Capture format HD -> adapt (OnOutputFormatRequest VGA) -> VGA.
  EXPECT_TRUE(video_capturer.CaptureFrame());
  const int channel0 = vie_.GetChannelFromLocalSsrc(kSsrcs3[0]);
  ASSERT_NE(-1, channel0);
  VerifyVP8SendCodec(channel0, vga_format.width, vga_format.height, 0,
                     kMaxBandwidthKbps, kMinBandwidthKbps, kStartBandwidthKbps,
                     30);
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs3[0], NULL));
}

TEST_F(WebRtcVideoEngineSimulcastTestFake, TestAdaptToCpuLoad) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs3))));

  // Capture format VGA
  cricket::FakeVideoCapturer video_capturer;
  const std::vector<cricket::VideoFormat>* formats =
      video_capturer.GetSupportedFormats();
  cricket::VideoFormat capture_format_vga = (*formats)[1];

  // Make OnCpuLoadUpdate trigger downgrade.
  cricket::VideoOptions options;
  options.adapt_input_to_cpu_usage.Set(true);
  options.process_adaptation_threshhold.Set(0.1f);
  options.system_low_adaptation_threshhold.Set(0.65f);
  options.system_high_adaptation_threshhold.Set(0.85f);
  options.cpu_overuse_detection.Set(false);
  EXPECT_TRUE(channel_->SetOptions(options));

  // Verify toggling cpu overuse detection works.
  options.cpu_overuse_detection.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  options.cpu_overuse_detection.Set(false);
  EXPECT_TRUE(channel_->SetOptions(options));

  EXPECT_EQ(cricket::CS_RUNNING, video_capturer.Start(capture_format_vga));
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs3[0], &video_capturer));

  // Only require one cpu sample for testing whether the adapter is hooked up.
  cricket::CoordinatedVideoAdapter* video_adapter = NULL;
  ASSERT_TRUE(channel_->GetVideoAdapter(0u, &video_adapter));
  video_adapter->set_cpu_load_min_samples(1);

  cpu_monitor_->SignalUpdate(1, 1, 0.1f, 0.85f);

  cricket::VideoCodec send_codec(100, "VP8", 640, 480, 30, 0);
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(send_codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_TRUE(channel_->SetSend(true));

  // Capture format VGA -> adapt (OnCpuLoadUpdate downgrade) -> VGA/2.
  EXPECT_TRUE(video_capturer.CaptureFrame());
  const int channel0 = vie_.GetChannelFromLocalSsrc(kSsrcs3[0]);
  ASSERT_NE(-1, channel0);
  VerifyVP8SendCodec(
      channel0, 3 * send_codec.width / 4, 3 * send_codec.height / 4, 0,
      kMaxBandwidthKbps, kMinBandwidthKbps, kStartBandwidthKbps,
      send_codec.framerate);

  // Trigger more downgrades and check for multiple unable to adapt signal.
  EXPECT_EQ(cricket::VideoMediaChannel::ERROR_NONE, last_error_);
  cpu_monitor_->SignalUpdate(1, 1, 0.1f, 0.85f);
  cpu_monitor_->SignalUpdate(1, 1, 0.1f, 0.85f);
  EXPECT_EQ(cricket::VideoMediaChannel::ERROR_REC_CPU_MAX_CANT_DOWNGRADE,
            last_error_);

  last_error_ = cricket::VideoMediaChannel::ERROR_NONE;
  cpu_monitor_->SignalUpdate(1, 1, 0.1f, 0.85f);
  EXPECT_EQ(cricket::VideoMediaChannel::ERROR_REC_CPU_MAX_CANT_DOWNGRADE,
            last_error_);

  EXPECT_TRUE(channel_->SetCapturer(kSsrcs3[0], NULL));
}

TEST_F(WebRtcVideoEngineSimulcastTestFake, TestAdaptToCpuLoadDisabled) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs3))));

  // Capture format VGA
  cricket::FakeVideoCapturer video_capturer;
  const std::vector<cricket::VideoFormat>* formats =
      video_capturer.GetSupportedFormats();
  cricket::VideoFormat capture_format_vga = (*formats)[1];
  EXPECT_EQ(cricket::CS_RUNNING, video_capturer.Start(capture_format_vga));
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs3[0], &video_capturer));

  // Make OnCpuLoadUpdate trigger downgrade.
  cricket::VideoOptions options;
  options.adapt_input_to_cpu_usage.Set(false);
  options.process_adaptation_threshhold.Set(0.1f);
  options.system_low_adaptation_threshhold.Set(0.65f);
  options.system_high_adaptation_threshhold.Set(0.85f);
  EXPECT_TRUE(channel_->SetOptions(options));

  // Only require one cpu sample for testing whether the adapter is hooked up.
  cricket::CoordinatedVideoAdapter* video_adapter = NULL;
  ASSERT_TRUE(channel_->GetVideoAdapter(0u, &video_adapter));
  video_adapter->set_cpu_load_min_samples(1);
  cpu_monitor_->SignalUpdate(1, 1, 0.1f, 0.85f);

  cricket::VideoCodec send_codec(100, "VP8", 640, 480, 30, 0);
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(send_codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_TRUE(channel_->SetSend(true));

  // Capture format VGA -> no adapt -> VGA.
  EXPECT_TRUE(video_capturer.CaptureFrame());
  const int channel0 = vie_.GetChannelFromLocalSsrc(kSsrcs3[0]);
  ASSERT_NE(-1, channel0);
  VerifyVP8SendCodec(channel0, capture_format_vga.width,
                     capture_format_vga.height, 0,
                     kMaxBandwidthKbps, kMinBandwidthKbps, kStartBandwidthKbps,
                     send_codec.framerate);
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs3[0], NULL));
}

TEST_F(WebRtcVideoEngineSimulcastTestFake, TestAdaptWithCpuOveruseObserver) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs3))));

  // Verify that the CpuOveruseObserver is not set by default.
  const int channel0 = vie_.GetChannelFromLocalSsrc(kSsrcs3[0]);
  ASSERT_NE(-1, channel0);
  webrtc::CpuOveruseObserver* observer = vie_.GetCpuOveruseObserver(channel0);
  EXPECT_TRUE(observer == NULL);

  // Capture format VGA.
  cricket::FakeVideoCapturer video_capturer;
  const std::vector<cricket::VideoFormat>* formats =
      video_capturer.GetSupportedFormats();
  cricket::VideoFormat capture_format_vga = (*formats)[1];
  EXPECT_EQ(cricket::CS_RUNNING, video_capturer.Start(capture_format_vga));
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs3[0], &video_capturer));

  // Verify that the CpuOveruseObserver is registered and trigger downgrade.
  cricket::VideoOptions options;
  options.cpu_overuse_detection.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  observer = vie_.GetCpuOveruseObserver(channel0);
  ASSERT_TRUE(observer != NULL);
  observer->OveruseDetected();

  cricket::VideoCodec send_codec(100, "VP8", 640, 480, 30, 0);
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(send_codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_TRUE(channel_->SetSend(true));

  // Capture format VGA -> adapt (OnCpuResolutionRequest downgrade) -> VGA/2.
  EXPECT_TRUE(video_capturer.CaptureFrame());
  VerifyVP8SendCodec(
      channel0, 3 * send_codec.width / 4, 3 * send_codec.height / 4, 0,
      kMaxBandwidthKbps, kMinBandwidthKbps, kStartBandwidthKbps,
      send_codec.framerate);

  // Trigger upgrade and verify that we adapt back up to VGA.
  observer->NormalUsage();
  EXPECT_TRUE(video_capturer.CaptureFrame());
  VerifyVP8SendCodec(
      channel0, send_codec.width, send_codec.height, 0,
      kMaxBandwidthKbps, kMinBandwidthKbps, kStartBandwidthKbps,
      send_codec.framerate);

  // Verify that the CpuOveruseObserver is deregistered.
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs3[0], NULL));
  EXPECT_TRUE(vie_.GetCpuOveruseObserver(channel0) == NULL);
}

TEST_F(WebRtcVideoEngineSimulcastTestFake,
       TestAdaptWithCpuOveruseObserverDisabled) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs3))));

  // Verify that the CpuOveruseObserver is not set by default.
  const int channel0 = vie_.GetChannelFromLocalSsrc(kSsrcs3[0]);
  ASSERT_NE(-1, channel0);
  webrtc::CpuOveruseObserver* observer = vie_.GetCpuOveruseObserver(channel0);
  EXPECT_TRUE(observer == NULL);

  // Capture format VGA.
  cricket::FakeVideoCapturer video_capturer;
  const std::vector<cricket::VideoFormat>* formats =
      video_capturer.GetSupportedFormats();
  cricket::VideoFormat capture_format_vga = (*formats)[1];
  EXPECT_EQ(cricket::CS_RUNNING, video_capturer.Start(capture_format_vga));
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs3[0], &video_capturer));

  // Disable cpu overuse detection.
  cricket::VideoOptions options;
  options.cpu_overuse_detection.Set(false);
  EXPECT_TRUE(channel_->SetOptions(options));
  cricket::CoordinatedVideoAdapter* video_adapter = NULL;
  ASSERT_TRUE(channel_->GetVideoAdapter(0u, &video_adapter));
  EXPECT_FALSE(video_adapter->cpu_adaptation());

  // Verify that the CpuOveruseObserver is registered and trigger downgrade.
  observer = vie_.GetCpuOveruseObserver(channel0);
  ASSERT_TRUE(observer != NULL);
  observer->OveruseDetected();

  cricket::VideoCodec send_codec(100, "VP8", 640, 480, 30, 0);
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(send_codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_TRUE(channel_->SetSend(true));

  // Capture format VGA -> no adapt -> VGA.
  EXPECT_TRUE(video_capturer.CaptureFrame());
  VerifyVP8SendCodec(
      channel0, capture_format_vga.width, capture_format_vga.height, 0,
      kMaxBandwidthKbps, kMinBandwidthKbps, kStartBandwidthKbps,
      send_codec.framerate);

  // Verify that the CpuOveruseObserver is deregistered.
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs3[0], NULL));
  EXPECT_TRUE(vie_.GetCpuOveruseObserver(channel0) == NULL);
}

TEST_F(WebRtcVideoEngineSimulcastTestFake, GetAdaptStats) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs3))));

  // Capture format VGA.
  cricket::FakeVideoCapturer video_capturer_vga;
  const std::vector<cricket::VideoFormat>* formats =
      video_capturer_vga.GetSupportedFormats();
  cricket::VideoFormat capture_format_vga = (*formats)[1];
  EXPECT_EQ(cricket::CS_RUNNING, video_capturer_vga.Start(capture_format_vga));
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs3[0], &video_capturer_vga));
  EXPECT_TRUE(video_capturer_vga.CaptureFrame());

  cricket::VideoCodec send_codec(100, "VP8", 640, 480, 30, 0);
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(send_codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_TRUE(channel_->SetSend(true));

  // Verify that the CpuOveruseObserver is registered and trigger downgrade.
  cricket::VideoOptions options;
  options.cpu_overuse_detection.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  const int channel0 = vie_.GetChannelFromLocalSsrc(kSsrcs3[0]);
  ASSERT_NE(-1, channel0);
  webrtc::CpuOveruseObserver* observer = vie_.GetCpuOveruseObserver(channel0);
  ASSERT_TRUE(observer != NULL);
  observer->OveruseDetected();

  // Capture format VGA -> adapt (OnCpuResolutionRequest downgrade) -> VGA/2.
  EXPECT_TRUE(video_capturer_vga.CaptureFrame());
  cricket::VideoMediaInfo info;
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.senders.size());
  EXPECT_EQ(1, info.senders[0].adapt_changes);
  EXPECT_EQ(cricket::CoordinatedVideoAdapter::ADAPTREASON_CPU,
            info.senders[0].adapt_reason);

  // Trigger upgrade and verify that we adapt back up to VGA.
  observer->NormalUsage();
  EXPECT_TRUE(video_capturer_vga.CaptureFrame());
  info.Clear();
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.senders.size());
  EXPECT_EQ(2, info.senders[0].adapt_changes);
  EXPECT_EQ(cricket::CoordinatedVideoAdapter::ADAPTREASON_NONE,
            info.senders[0].adapt_reason);

  // No capturer (no adapter). Adapt changes from old adapter should be kept.
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs3[0], NULL));
  EXPECT_TRUE(vie_.GetCpuOveruseObserver(channel0) == NULL);
  info.Clear();
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.senders.size());
  EXPECT_EQ(2, info.senders[0].adapt_changes);
  EXPECT_EQ(cricket::CoordinatedVideoAdapter::ADAPTREASON_NONE,
      info.senders[0].adapt_reason);

  // Set new capturer, capture format HD.
  cricket::FakeVideoCapturer video_capturer_hd;
  cricket::VideoFormat capture_format_hd = (*formats)[0];
  EXPECT_EQ(cricket::CS_RUNNING, video_capturer_hd.Start(capture_format_hd));
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs3[0], &video_capturer_hd));
  EXPECT_TRUE(video_capturer_hd.CaptureFrame());
  observer = vie_.GetCpuOveruseObserver(channel0);
  ASSERT_TRUE(observer != NULL);

  // Trigger overuse, HD -> adapt (OnCpuResolutionRequest downgrade) -> HD/2.
  observer->OveruseDetected();
  EXPECT_TRUE(video_capturer_hd.CaptureFrame());
  info.Clear();
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.senders.size());
  EXPECT_EQ(3, info.senders[0].adapt_changes);
  EXPECT_EQ(cricket::CoordinatedVideoAdapter::ADAPTREASON_CPU,
      info.senders[0].adapt_reason);

  EXPECT_TRUE(channel_->SetCapturer(kSsrcs3[0], NULL));
}

// Test that the codec is not reset for every frame sent in
// non-conference and non-screencast mode.
TEST_F(WebRtcVideoEngineSimulcastTestFake, DontResetCodecOnSendFrame) {
  EXPECT_TRUE(SetupEngine());

  // Set send codec.
  cricket::VideoCodec codec(kVP8Codec);
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(codec);
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(123)));
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_EQ(1, vie_.GetNumSetSendCodecs());

  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_EQ(1, vie_.GetNumSetSendCodecs());
  SendI420Frame(kVP8Codec.width, kVP8Codec.height);
  EXPECT_EQ(1, vie_.GetNumSetSendCodecs());
}

TEST_F(WebRtcVideoEngineSimulcastTestFake,
       UseSimulcastAdapterOnVp8OnlyFactory) {
  encoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecVP8, "VP8");
  TestSimulcastAdapter(kVP8Codec, true);
}

TEST_F(WebRtcVideoEngineSimulcastTestFake,
       UsesSimulcastAdapterForVp8WithCombinedVP8AndH264Factory) {
  encoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecVP8, "VP8");
  encoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecGeneric,
                                              "H264");
  TestSimulcastAdapter(kVP8Codec, true);
}

TEST_F(WebRtcVideoEngineSimulcastTestFake,
       DontUseSimulcastAdapterForH264WithCombinedVP8AndH264Factory) {
  encoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecVP8, "VP8");
  encoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecGeneric,
                                              "H264");
  static const cricket::VideoCodec kH264Codec(100, "H264", 640, 400, 30, 0);
  TestSimulcastAdapter(kH264Codec, false);
}

TEST_F(WebRtcVideoEngineSimulcastTestFake,
       DontUseSimulcastAdapterOnNonVp8Factory) {
  encoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecGeneric,
                                              "H264");
  static const cricket::VideoCodec kH264Codec(100, "H264", 640, 400, 30, 0);
  TestSimulcastAdapter(kH264Codec, false);
}

// Flaky on Windows and tsan. https://code.google.com/p/webrtc/issues/detail?id=4135
#if defined(WIN32) || defined(THREAD_SANITIZER)
#define MAYBE_SimulcastSend_1280x800 DISABLED_SimulcastSend_1280x800
#else
#define MAYBE_SimulcastSend_1280x800 SimulcastSend_1280x800
#endif
TEST_F(WebRtcVideoMediaChannelSimulcastTest, MAYBE_SimulcastSend_1280x800) {
  cricket::VideoCodec codec = kVP8Codec;
  codec.width = 1280;
  codec.height = 800;
  // TODO(zhurunz): Support 3 layers of simulcast.
  SimulcastSend(codec, MAKE_VECTOR(kSsrcs2));
}

// Flaky on Windows and tsan. https://code.google.com/p/webrtc/issues/detail?id=4135
#if defined(WIN32) || defined(THREAD_SANITIZER)
#define MAYBE_SimulcastSend_1280x720 DISABLED_SimulcastSend_1280x720
#else
#define MAYBE_SimulcastSend_1280x720 SimulcastSend_1280x720
#endif
TEST_F(WebRtcVideoMediaChannelSimulcastTest, MAYBE_SimulcastSend_1280x720) {
  cricket::VideoCodec codec = kVP8Codec;
  codec.width = 1280;
  codec.height = 720;
  // TODO(zhurunz): Support 3 layers of simulcast.
  SimulcastSend(codec, MAKE_VECTOR(kSsrcs2));
}

// Flaky on Windows and tsan. https://code.google.com/p/webrtc/issues/detail?id=4135
#if defined(WIN32) || defined(THREAD_SANITIZER)
#define MAYBE_SimulcastSend_960x540 DISABLED_SimulcastSend_960x540
#else
#define MAYBE_SimulcastSend_960x540 SimulcastSend_960x540
#endif
TEST_F(WebRtcVideoMediaChannelSimulcastTest, MAYBE_SimulcastSend_960x540) {
  cricket::VideoCodec codec = kVP8Codec;
  codec.width = 960;
  codec.height = 540;
  // TODO(zhurunz): Support 3 layers of simulcast.
  SimulcastSend(codec, MAKE_VECTOR(kSsrcs2));
}

// Flaky on Windows and tsan. https://code.google.com/p/webrtc/issues/detail?id=4135
#if defined(WIN32) || defined(THREAD_SANITIZER)
#define MAYBE_SimulcastSend_960x600 DISABLED_SimulcastSend_960x600
#else
#define MAYBE_SimulcastSend_960x600 SimulcastSend_960x600
#endif
TEST_F(WebRtcVideoMediaChannelSimulcastTest, MAYBE_SimulcastSend_960x600) {
  cricket::VideoCodec codec = kVP8Codec;
  codec.width = 960;
  codec.height = 600;
  // TODO(zhurunz): Support 3 layers of simulcast.
  SimulcastSend(codec, MAKE_VECTOR(kSsrcs2));
}

// Flaky on Windows and tsan. https://code.google.com/p/webrtc/issues/detail?id=4135
#if defined(WIN32) || defined(THREAD_SANITIZER)
#define MAYBE_SimulcastSend_640x400 DISABLED_SimulcastSend_640x400
#else
#define MAYBE_SimulcastSend_640x400 SimulcastSend_640x400
#endif
TEST_F(WebRtcVideoMediaChannelSimulcastTest, MAYBE_SimulcastSend_640x400) {
  cricket::VideoCodec codec = kVP8Codec;
  codec.width = 640;
  codec.height = 400;
  SimulcastSend(codec, MAKE_VECTOR(kSsrcs2));
}

// Flaky on Windows and tsan. https://code.google.com/p/webrtc/issues/detail?id=4135
#if defined(WIN32) || defined(THREAD_SANITIZER)
#define MAYBE_SimulcastSend_640x360 DISABLED_SimulcastSend_640x360
#else
#define MAYBE_SimulcastSend_640x360 SimulcastSend_640x360
#endif
TEST_F(WebRtcVideoMediaChannelSimulcastTest, MAYBE_SimulcastSend_640x360) {
  cricket::VideoCodec codec = kVP8Codec;
  codec.width = 640;
  codec.height = 360;
  SimulcastSend(codec, MAKE_VECTOR(kSsrcs2));
}

// Flaky on Windows and tsan. https://code.google.com/p/webrtc/issues/detail?id=4135
#if defined(WIN32) || defined(THREAD_SANITIZER)
#define MAYBE_SimulcastSend_480x300 DISABLED_SimulcastSend_480x300
#else
#define MAYBE_SimulcastSend_480x300 SimulcastSend_480x300
#endif
TEST_F(WebRtcVideoMediaChannelSimulcastTest, MAYBE_SimulcastSend_480x300) {
  cricket::VideoCodec codec = kVP8Codec;
  codec.width = 480;
  codec.height = 300;
  SimulcastSend(codec, MAKE_VECTOR(kSsrcs2));
}

TEST_F(WebRtcVideoMediaChannelSimulcastTest, DISABLED_SimulcastSend_480x270) {
  cricket::VideoCodec codec = kVP8Codec;
  codec.width = 480;
  codec.height = 270;
  SimulcastSend(codec, MAKE_VECTOR(kSsrcs2));
}

// Flaky on Windows and tsan. https://code.google.com/p/webrtc/issues/detail?id=4135
#if defined(WIN32) || defined(THREAD_SANITIZER)
#define MAYBE_SimulcastSend_320x200 DISABLED_SimulcastSend_320x200
#else
#define MAYBE_SimulcastSend_320x200 SimulcastSend_320x200
#endif
TEST_F(WebRtcVideoMediaChannelSimulcastTest, MAYBE_SimulcastSend_320x200) {
  cricket::VideoCodec codec = kVP8Codec;
  codec.width = 320;
  codec.height = 200;
  SimulcastSend(codec, MAKE_VECTOR(kSsrcs1));
}

// Flaky on Windows and tsan. https://code.google.com/p/webrtc/issues/detail?id=4135
#if defined(WIN32) || defined(THREAD_SANITIZER)
#define MAYBE_SimulcastSend_320x180 DISABLED_SimulcastSend_320x180
#else
#define MAYBE_SimulcastSend_320x180 SimulcastSend_320x180
#endif
TEST_F(WebRtcVideoMediaChannelSimulcastTest, MAYBE_SimulcastSend_320x180) {
  cricket::VideoCodec codec = kVP8Codec;
  codec.width = 320;
  codec.height = 180;
  SimulcastSend(codec, MAKE_VECTOR(kSsrcs1));
}

// Test reset send codec with simulcast.
// Disabled per b/6773425
TEST_F(WebRtcVideoMediaChannelSimulcastTest, DISABLED_SimulcastResetSendCodec) {
  // Remove stream added in Setup.
  EXPECT_TRUE(channel_->RemoveSendStream(kSsrc));

  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs2))));
  cricket::VideoCodec codec(DefaultCodec());
  EXPECT_TRUE(SetOneCodec(codec));
  EXPECT_TRUE(SetSend(true));
  int packets = 0;

  // Send the first frame with default format size.
  EXPECT_TRUE(SendFrame());
  EXPECT_TRUE_WAIT(NumRtpPackets() > packets, kTimeout);
  DrainOutgoingPackets();
  packets = NumRtpPackets();

  // Simulate capture frame size changes.
  // Verify there is new rtp packet coming out of the encoder.

  // Send an odd frame.
  EXPECT_TRUE(SendCustomVideoFrame(DefaultCodec().width | 0x01,
                                   DefaultCodec().height | 0x01));
  EXPECT_TRUE_WAIT(NumRtpPackets() > packets, kTimeout);
  packets = DrainOutgoingPackets();

  // Send a small frame in width.
  EXPECT_TRUE(SendCustomVideoFrame(DefaultCodec().width / 2,
                                   DefaultCodec().height));
  EXPECT_TRUE_WAIT(NumRtpPackets() > packets, kTimeout);
  packets = DrainOutgoingPackets();

  // Send a small frame in height.
  EXPECT_TRUE(SendCustomVideoFrame(DefaultCodec().width,
                                   DefaultCodec().height / 2));
  EXPECT_TRUE_WAIT(NumRtpPackets() > packets, kTimeout);
  packets = DrainOutgoingPackets();

  // Send a big frame in width.
  EXPECT_TRUE(SendCustomVideoFrame(DefaultCodec().width * 2,
                                   DefaultCodec().height));
  EXPECT_TRUE_WAIT(NumRtpPackets() > packets, kTimeout);
  packets = DrainOutgoingPackets();

  // Send a big frame in height.
  EXPECT_TRUE(SendCustomVideoFrame(DefaultCodec().width,
                                   DefaultCodec().height * 2));
  EXPECT_TRUE_WAIT(NumRtpPackets() > packets, kTimeout);
  packets = DrainOutgoingPackets();

  // Send a normal frame.
  EXPECT_TRUE(SendCustomVideoFrame(DefaultCodec().width,
                                   DefaultCodec().height));
  EXPECT_TRUE_WAIT(NumRtpPackets() > packets, kTimeout);
  packets = DrainOutgoingPackets();
}

// Test simulcast streams are decodeable with expected sizes.
TEST_F(WebRtcVideoMediaChannelSimulcastTest, SimulcastStreams) {
  // Remove stream added in Setup.
  EXPECT_TRUE(channel_->RemoveSendStream(kSsrc));

  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs2))));
  cricket::VideoCodec codec(DefaultCodec());
  EXPECT_TRUE(SetOneCodec(codec));

  // Re-configure capturer.
  channel_->SetCapturer(kSsrc, NULL);
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs2[0], video_capturer_.get()));
  cricket::VideoFormat capture_format(codec.width, codec.height,
      cricket::VideoFormat::FpsToInterval(codec.framerate),
      cricket::FOURCC_I420);
  EXPECT_EQ(cricket::CS_RUNNING, video_capturer_->Start(capture_format));

  EXPECT_TRUE(SetSend(true));

  cricket::FakeVideoRenderer renderer1, renderer2;
  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::StreamParams::CreateLegacy(kSsrcs2[0])));
  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::StreamParams::CreateLegacy(kSsrcs2[1])));
  EXPECT_TRUE(channel_->SetRenderer(kSsrcs2[0], &renderer1));
  EXPECT_TRUE(channel_->SetRenderer(kSsrcs2[1], &renderer2));
  EXPECT_TRUE(channel_->SetRender(true));
  EXPECT_EQ(0, renderer1.num_rendered_frames());
  EXPECT_EQ(0, renderer2.num_rendered_frames());

  EXPECT_TRUE(SendFrame());
  EXPECT_FRAME_ON_RENDERER_WAIT(renderer1, 1,
      DefaultCodec().width / 2, DefaultCodec().height / 2, kTimeout);
  EXPECT_FRAME_ON_RENDERER_WAIT(renderer2, 1,
      DefaultCodec().width, DefaultCodec().height, kTimeout);
  EXPECT_EQ(1, renderer1.num_set_sizes());
  EXPECT_EQ(1, renderer2.num_set_sizes());
}

// Simulcast and resolution resizing should be turned off when screencasting
// but not otherwise.
// TODO(dmunene): Fix flaky test with bug 15773660.
TEST_F(WebRtcVideoMediaChannelSimulcastTest, DISABLED_ScreencastRendering) {
  // Remove stream added in SetUp.
  EXPECT_TRUE(channel_->RemoveSendStream(kSsrc));

  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));

  // Setup a simulcast stream and verify that frames are rendered on
  // both renderers.
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs2))));
  cricket::VideoCodec codec(DefaultCodec());
  EXPECT_TRUE(SetOneCodec(codec));
  EXPECT_TRUE(SetSend(true));

  cricket::FakeVideoRenderer renderer1, renderer2;
  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::StreamParams::CreateLegacy(kSsrcs2[0])));
  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::StreamParams::CreateLegacy(kSsrcs2[1])));
  EXPECT_TRUE(channel_->SetRenderer(kSsrcs2[0], &renderer1));
  EXPECT_TRUE(channel_->SetRenderer(kSsrcs2[1], &renderer2));
  EXPECT_TRUE(channel_->SetRender(true));
  EXPECT_EQ(0, renderer1.num_rendered_frames());
  EXPECT_EQ(0, renderer2.num_rendered_frames());

  // Re-configure capturer.
  channel_->SetCapturer(kSsrc, NULL);
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs2[0], video_capturer_.get()));
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs2[1], video_capturer_.get()));
  cricket::VideoFormat capture_format(codec.width, codec.height,
      cricket::VideoFormat::FpsToInterval(codec.framerate),
      cricket::FOURCC_I420);
  EXPECT_EQ(cricket::CS_RUNNING, video_capturer_->Start(capture_format));

  EXPECT_TRUE(SendFrame());
  EXPECT_FRAME_ON_RENDERER_WAIT(renderer1, 1, codec.width / 2, codec.height / 2,
                                kTimeout);
  EXPECT_FRAME_ON_RENDERER_WAIT(renderer2, 1, codec.width, codec.height,
                                kTimeout);

  // Register a fake screencast capturer and verify that frames are only
  // rendered to a single renderer.
  rtc::scoped_ptr<cricket::FakeVideoCapturer> capturer(
      new cricket::FakeVideoCapturer);
  capturer->SetScreencast(true);
  const std::vector<cricket::VideoFormat>* formats =
      capturer->GetSupportedFormats();
  capture_format = (*formats)[0];
  EXPECT_EQ(cricket::CS_RUNNING, capturer->Start(capture_format));
  // Capture a frame to increment the frame timestamp since the default video
  // capturer starts at the same timestamp.
  EXPECT_TRUE(capturer->CaptureFrame());
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs2[0], capturer.get()));
  EXPECT_TRUE(rtc::Thread::Current()->ProcessMessages(30));
  EXPECT_TRUE(capturer->CaptureFrame());
  EXPECT_FRAME_ON_RENDERER_WAIT(renderer1, 2, capture_format.width,
                                capture_format.height, kTimeout);
  // When screencasting we only encode a single stream (no simulcast), which is
  // why we only expect frames to be rendered in one of the renderers.
  EXPECT_EQ(1, renderer2.num_rendered_frames());

  // Disable screencast and make sure frames are now rendered in both renderers.
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs2[0], NULL));
  EXPECT_FRAME_ON_RENDERER_WAIT(renderer1, 3, codec.width / 2, codec.height / 2,
                                kTimeout);
  EXPECT_FRAME_ON_RENDERER_WAIT(renderer2, 2, codec.width, codec.height,
                                kTimeout);
}

// Disable for TSan v2, see
// https://code.google.com/p/webrtc/issues/detail?id=3525 for details.
#if !defined(THREAD_SANITIZER)
// Ensures that the correct settings are applied to the codec when two temporal
// layer screencasting is enabled, and that the correct simulcast settings are
// reapplied when disabling screencasting.
TEST_F(WebRtcVideoMediaChannelSimulcastTest, ConferenceModeScreencastSettings) {
  TestScreencastSettings();
}
#endif  // if !defined(THREAD_SANITIZER)
