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
#include <map>
#include <vector>

#include "talk/media/base/testutils.h"
#include "talk/media/base/videoengine_unittest.h"
#include "talk/media/webrtc/fakewebrtccall.h"
#include "talk/media/webrtc/fakewebrtcvideoengine.h"
#include "talk/media/webrtc/simulcast.h"
#include "talk/media/webrtc/webrtcvideochannelfactory.h"
#include "talk/media/webrtc/webrtcvideoengine2.h"
#include "talk/media/webrtc/webrtcvoiceengine.h"
#include "webrtc/base/arraysize.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/test/field_trial.h"
#include "webrtc/video_encoder.h"

namespace {
static const int kDefaultQpMax = 56;
static const int kDefaultFramerate = 30;

static const cricket::VideoCodec kVp8Codec720p(100, "VP8", 1280, 720, 30, 0);
static const cricket::VideoCodec kVp8Codec360p(100, "VP8", 640, 360, 30, 0);
static const cricket::VideoCodec kVp8Codec270p(100, "VP8", 480, 270, 30, 0);

static const cricket::VideoCodec kVp8Codec(100, "VP8", 640, 400, 30, 0);
static const cricket::VideoCodec kVp9Codec(101, "VP9", 640, 400, 30, 0);
static const cricket::VideoCodec kH264Codec(102, "H264", 640, 400, 30, 0);

static const cricket::VideoCodec kRedCodec(116, "red", 0, 0, 0, 0);
static const cricket::VideoCodec kUlpfecCodec(117, "ulpfec", 0, 0, 0, 0);

static const uint8_t kRedRtxPayloadType = 125;

static const uint32_t kSsrcs1[] = {1};
static const uint32_t kSsrcs3[] = {1, 2, 3};
static const uint32_t kRtxSsrcs1[] = {4};
static const uint32_t kIncomingUnsignalledSsrc = 0xC0FFEE;
static const char kUnsupportedExtensionName[] =
    "urn:ietf:params:rtp-hdrext:unsupported";

void VerifyCodecHasDefaultFeedbackParams(const cricket::VideoCodec& codec) {
  EXPECT_TRUE(codec.HasFeedbackParam(cricket::FeedbackParam(
      cricket::kRtcpFbParamNack, cricket::kParamValueEmpty)));
  EXPECT_TRUE(codec.HasFeedbackParam(cricket::FeedbackParam(
      cricket::kRtcpFbParamNack, cricket::kRtcpFbNackParamPli)));
  EXPECT_TRUE(codec.HasFeedbackParam(cricket::FeedbackParam(
      cricket::kRtcpFbParamRemb, cricket::kParamValueEmpty)));
  EXPECT_TRUE(codec.HasFeedbackParam(cricket::FeedbackParam(
      cricket::kRtcpFbParamTransportCc, cricket::kParamValueEmpty)));
  EXPECT_TRUE(codec.HasFeedbackParam(cricket::FeedbackParam(
      cricket::kRtcpFbParamCcm, cricket::kRtcpFbCcmParamFir)));
}

static void CreateBlackFrame(webrtc::VideoFrame* video_frame,
                             int width,
                             int height) {
  video_frame->CreateEmptyFrame(
      width, height, width, (width + 1) / 2, (width + 1) / 2);
  memset(video_frame->buffer(webrtc::kYPlane), 16,
         video_frame->allocated_size(webrtc::kYPlane));
  memset(video_frame->buffer(webrtc::kUPlane), 128,
         video_frame->allocated_size(webrtc::kUPlane));
  memset(video_frame->buffer(webrtc::kVPlane), 128,
         video_frame->allocated_size(webrtc::kVPlane));
}

void VerifySendStreamHasRtxTypes(const webrtc::VideoSendStream::Config& config,
                                 const std::map<int, int>& rtx_types) {
  std::map<int, int>::const_iterator it;
  it = rtx_types.find(config.encoder_settings.payload_type);
  EXPECT_TRUE(it != rtx_types.end() &&
              it->second == config.rtp.rtx.payload_type);

  if (config.rtp.fec.red_rtx_payload_type != -1) {
    it = rtx_types.find(config.rtp.fec.red_payload_type);
    EXPECT_TRUE(it != rtx_types.end() &&
                it->second == config.rtp.fec.red_rtx_payload_type);
  }
}
}  // namespace

namespace cricket {
class WebRtcVideoEngine2Test : public ::testing::Test {
 public:
  WebRtcVideoEngine2Test() : WebRtcVideoEngine2Test("") {}
  explicit WebRtcVideoEngine2Test(const char* field_trials)
      : WebRtcVideoEngine2Test(nullptr, field_trials) {}
  WebRtcVideoEngine2Test(WebRtcVoiceEngine* voice_engine,
                         const char* field_trials)
      : override_field_trials_(field_trials),
        call_(webrtc::Call::Create(webrtc::Call::Config())),
        engine_() {
    std::vector<VideoCodec> engine_codecs = engine_.codecs();
    RTC_DCHECK(!engine_codecs.empty());
    bool codec_set = false;
    for (size_t i = 0; i < engine_codecs.size(); ++i) {
      if (engine_codecs[i].name == "red") {
        default_red_codec_ = engine_codecs[i];
      } else if (engine_codecs[i].name == "ulpfec") {
        default_ulpfec_codec_ = engine_codecs[i];
      } else if (engine_codecs[i].name == "rtx") {
        int associated_payload_type;
        if (engine_codecs[i].GetParam(kCodecParamAssociatedPayloadType,
                                      &associated_payload_type)) {
          default_apt_rtx_types_[associated_payload_type] = engine_codecs[i].id;
        }
      } else if (!codec_set) {
        default_codec_ = engine_codecs[i];
        codec_set = true;
      }
    }

    RTC_DCHECK(codec_set);
  }

 protected:
  VideoMediaChannel* SetUpForExternalEncoderFactory(
      cricket::WebRtcVideoEncoderFactory* encoder_factory,
      const std::vector<VideoCodec>& codecs);

  VideoMediaChannel* SetUpForExternalDecoderFactory(
      cricket::WebRtcVideoDecoderFactory* decoder_factory,
      const std::vector<VideoCodec>& codecs);

  webrtc::test::ScopedFieldTrials override_field_trials_;
  // Used in WebRtcVideoEngine2VoiceTest, but defined here so it's properly
  // initialized when the constructor is called.
  rtc::scoped_ptr<webrtc::Call> call_;
  WebRtcVoiceEngine voice_engine_;
  WebRtcVideoEngine2 engine_;
  VideoCodec default_codec_;
  VideoCodec default_red_codec_;
  VideoCodec default_ulpfec_codec_;
  std::map<int, int> default_apt_rtx_types_;
};

TEST_F(WebRtcVideoEngine2Test, FindCodec) {
  const std::vector<cricket::VideoCodec>& c = engine_.codecs();
  EXPECT_EQ(cricket::DefaultVideoCodecList().size(), c.size());

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

  // FindCodec ignores the codec size.
  // Test that FindCodec can accept uncommon codec size.
  cricket::VideoCodec vp8_diff_res(104, "VP8", 320, 111, 30, 0);
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
  EXPECT_TRUE(engine_.FindCodec(rtx));
}

TEST_F(WebRtcVideoEngine2Test, DefaultRtxCodecHasAssociatedPayloadTypeSet) {
  std::vector<VideoCodec> engine_codecs = engine_.codecs();
  for (size_t i = 0; i < engine_codecs.size(); ++i) {
    if (engine_codecs[i].name != kRtxCodecName)
      continue;
    int associated_payload_type;
    EXPECT_TRUE(engine_codecs[i].GetParam(kCodecParamAssociatedPayloadType,
                                          &associated_payload_type));
    EXPECT_EQ(default_codec_.id, associated_payload_type);
    return;
  }
  FAIL() << "No RTX codec found among default codecs.";
}

TEST_F(WebRtcVideoEngine2Test, SupportsTimestampOffsetHeaderExtension) {
  RtpCapabilities capabilities = engine_.GetCapabilities();
  ASSERT_FALSE(capabilities.header_extensions.empty());
  for (const RtpHeaderExtension& extension : capabilities.header_extensions) {
    if (extension.uri == kRtpTimestampOffsetHeaderExtension) {
      EXPECT_EQ(kRtpTimestampOffsetHeaderExtensionDefaultId, extension.id);
      return;
    }
  }
  FAIL() << "Timestamp offset extension not in header-extension list.";
}

TEST_F(WebRtcVideoEngine2Test, SupportsAbsoluteSenderTimeHeaderExtension) {
  RtpCapabilities capabilities = engine_.GetCapabilities();
  ASSERT_FALSE(capabilities.header_extensions.empty());
  for (const RtpHeaderExtension& extension : capabilities.header_extensions) {
    if (extension.uri == kRtpAbsoluteSenderTimeHeaderExtension) {
      EXPECT_EQ(kRtpAbsoluteSenderTimeHeaderExtensionDefaultId, extension.id);
      return;
    }
  }
  FAIL() << "Absolute Sender Time extension not in header-extension list.";
}

class WebRtcVideoEngine2WithSendSideBweTest : public WebRtcVideoEngine2Test {
 public:
  WebRtcVideoEngine2WithSendSideBweTest()
      : WebRtcVideoEngine2Test("WebRTC-SendSideBwe/Enabled/") {}
};

TEST_F(WebRtcVideoEngine2WithSendSideBweTest,
       SupportsTransportSequenceNumberHeaderExtension) {
  RtpCapabilities capabilities = engine_.GetCapabilities();
  ASSERT_FALSE(capabilities.header_extensions.empty());
  for (const RtpHeaderExtension& extension : capabilities.header_extensions) {
    if (extension.uri == kRtpTransportSequenceNumberHeaderExtension) {
      EXPECT_EQ(kRtpTransportSequenceNumberHeaderExtensionDefaultId,
                extension.id);
      return;
    }
  }
  FAIL() << "Transport sequence number extension not in header-extension list.";
}

TEST_F(WebRtcVideoEngine2Test, SupportsVideoRotationHeaderExtension) {
  RtpCapabilities capabilities = engine_.GetCapabilities();
  ASSERT_FALSE(capabilities.header_extensions.empty());
  for (const RtpHeaderExtension& extension : capabilities.header_extensions) {
    if (extension.uri == kRtpVideoRotationHeaderExtension) {
      EXPECT_EQ(kRtpVideoRotationHeaderExtensionDefaultId, extension.id);
      return;
    }
  }
  FAIL() << "Video Rotation extension not in header-extension list.";
}

TEST_F(WebRtcVideoEngine2Test, CVOSetHeaderExtensionBeforeCapturer) {
  // Allocate the capturer first to prevent early destruction before channel's
  // dtor is called.
  cricket::FakeVideoCapturer capturer;

  cricket::FakeWebRtcVideoEncoderFactory encoder_factory;
  encoder_factory.AddSupportedVideoCodecType(webrtc::kVideoCodecVP8, "VP8");
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(kVp8Codec);

  rtc::scoped_ptr<VideoMediaChannel> channel(
      SetUpForExternalEncoderFactory(&encoder_factory, parameters.codecs));
  EXPECT_TRUE(channel->AddSendStream(StreamParams::CreateLegacy(kSsrc)));

  // Add CVO extension.
  const int id = 1;
  parameters.extensions.push_back(
      cricket::RtpHeaderExtension(kRtpVideoRotationHeaderExtension, id));
  EXPECT_TRUE(channel->SetSendParameters(parameters));

  // Set capturer.
  EXPECT_TRUE(channel->SetCapturer(kSsrc, &capturer));

  // Verify capturer has turned off applying rotation.
  EXPECT_FALSE(capturer.GetApplyRotation());

  // Verify removing header extension turns on applying rotation.
  parameters.extensions.clear();
  EXPECT_TRUE(channel->SetSendParameters(parameters));
  EXPECT_TRUE(capturer.GetApplyRotation());
}

TEST_F(WebRtcVideoEngine2Test, CVOSetHeaderExtensionAfterCapturer) {
  cricket::FakeVideoCapturer capturer;

  cricket::FakeWebRtcVideoEncoderFactory encoder_factory;
  encoder_factory.AddSupportedVideoCodecType(webrtc::kVideoCodecVP8, "VP8");
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(kVp8Codec);

  rtc::scoped_ptr<VideoMediaChannel> channel(
      SetUpForExternalEncoderFactory(&encoder_factory, parameters.codecs));
  EXPECT_TRUE(channel->AddSendStream(StreamParams::CreateLegacy(kSsrc)));

  // Set capturer.
  EXPECT_TRUE(channel->SetCapturer(kSsrc, &capturer));

  // Add CVO extension.
  const int id = 1;
  parameters.extensions.push_back(
      cricket::RtpHeaderExtension(kRtpVideoRotationHeaderExtension, id));
  EXPECT_TRUE(channel->SetSendParameters(parameters));

  // Verify capturer has turned off applying rotation.
  EXPECT_FALSE(capturer.GetApplyRotation());

  // Verify removing header extension turns on applying rotation.
  parameters.extensions.clear();
  EXPECT_TRUE(channel->SetSendParameters(parameters));
  EXPECT_TRUE(capturer.GetApplyRotation());
}

TEST_F(WebRtcVideoEngine2Test, SetSendFailsBeforeSettingCodecs) {
  engine_.Init();
  rtc::scoped_ptr<VideoMediaChannel> channel(
      engine_.CreateChannel(call_.get(), cricket::VideoOptions()));

  EXPECT_TRUE(channel->AddSendStream(StreamParams::CreateLegacy(123)));

  EXPECT_FALSE(channel->SetSend(true))
      << "Channel should not start without codecs.";
  EXPECT_TRUE(channel->SetSend(false))
      << "Channel should be stoppable even without set codecs.";
}

TEST_F(WebRtcVideoEngine2Test, GetStatsWithoutSendCodecsSetDoesNotCrash) {
  engine_.Init();
  rtc::scoped_ptr<VideoMediaChannel> channel(
      engine_.CreateChannel(call_.get(), cricket::VideoOptions()));
  EXPECT_TRUE(channel->AddSendStream(StreamParams::CreateLegacy(123)));
  VideoMediaInfo info;
  channel->GetStats(&info);
}

TEST_F(WebRtcVideoEngine2Test, UseExternalFactoryForVp8WhenSupported) {
  cricket::FakeWebRtcVideoEncoderFactory encoder_factory;
  encoder_factory.AddSupportedVideoCodecType(webrtc::kVideoCodecVP8, "VP8");
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(kVp8Codec);

  rtc::scoped_ptr<VideoMediaChannel> channel(
      SetUpForExternalEncoderFactory(&encoder_factory, parameters.codecs));

  EXPECT_TRUE(
      channel->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc)));
  ASSERT_EQ(1u, encoder_factory.encoders().size());
  EXPECT_TRUE(channel->SetSend(true));

  cricket::FakeVideoCapturer capturer;
  EXPECT_TRUE(channel->SetCapturer(kSsrc, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  EXPECT_TRUE(capturer.CaptureFrame());
  EXPECT_TRUE_WAIT(encoder_factory.encoders()[0]->GetNumEncodedFrames() > 0,
                   kTimeout);

  // Sending one frame will have reallocated the encoder since input size
  // changes from a small default to the actual frame width/height.
  int num_created_encoders = encoder_factory.GetNumCreatedEncoders();
  EXPECT_EQ(num_created_encoders, 2);

  // Setting codecs of the same type should not reallocate any encoders
  // (expecting a no-op).
  EXPECT_TRUE(channel->SetSendParameters(parameters));
  EXPECT_EQ(num_created_encoders, encoder_factory.GetNumCreatedEncoders());

  // Remove stream previously added to free the external encoder instance.
  EXPECT_TRUE(channel->RemoveSendStream(kSsrc));
  EXPECT_EQ(0u, encoder_factory.encoders().size());
}

TEST_F(WebRtcVideoEngine2Test, CanConstructDecoderForVp9EncoderFactory) {
  cricket::FakeWebRtcVideoEncoderFactory encoder_factory;
  encoder_factory.AddSupportedVideoCodecType(webrtc::kVideoCodecVP9, "VP9");
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVp9Codec);

  rtc::scoped_ptr<VideoMediaChannel> channel(
      SetUpForExternalEncoderFactory(&encoder_factory, codecs));

  EXPECT_TRUE(
      channel->AddRecvStream(cricket::StreamParams::CreateLegacy(kSsrc)));
}

TEST_F(WebRtcVideoEngine2Test, PropagatesInputFrameTimestamp) {
  cricket::FakeWebRtcVideoEncoderFactory encoder_factory;
  encoder_factory.AddSupportedVideoCodecType(webrtc::kVideoCodecVP8, "VP8");
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVp8Codec);

  FakeCall* fake_call = new FakeCall(webrtc::Call::Config());
  call_.reset(fake_call);
  rtc::scoped_ptr<VideoMediaChannel> channel(
      SetUpForExternalEncoderFactory(&encoder_factory, codecs));

  EXPECT_TRUE(
      channel->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc)));

  FakeVideoCapturer capturer;
  EXPECT_TRUE(channel->SetCapturer(kSsrc, &capturer));
  capturer.Start(cricket::VideoFormat(1280, 720,
                                      cricket::VideoFormat::FpsToInterval(60),
                                      cricket::FOURCC_I420));
  channel->SetSend(true);

  FakeVideoSendStream* stream = fake_call->GetVideoSendStreams()[0];

  EXPECT_TRUE(capturer.CaptureFrame());
  int64_t last_timestamp = stream->GetLastTimestamp();
  for (int i = 0; i < 10; i++) {
    EXPECT_TRUE(capturer.CaptureFrame());
    int64_t timestamp = stream->GetLastTimestamp();
    int64_t interval = timestamp - last_timestamp;

    // Precision changes from nanosecond to millisecond.
    // Allow error to be no more than 1.
    EXPECT_NEAR(cricket::VideoFormat::FpsToInterval(60) / 1E6, interval, 1);

    last_timestamp = timestamp;
  }

  capturer.Start(cricket::VideoFormat(1280, 720,
                                      cricket::VideoFormat::FpsToInterval(30),
                                      cricket::FOURCC_I420));

  EXPECT_TRUE(capturer.CaptureFrame());
  last_timestamp = stream->GetLastTimestamp();
  for (int i = 0; i < 10; i++) {
    EXPECT_TRUE(capturer.CaptureFrame());
    int64_t timestamp = stream->GetLastTimestamp();
    int64_t interval = timestamp - last_timestamp;

    // Precision changes from nanosecond to millisecond.
    // Allow error to be no more than 1.
    EXPECT_NEAR(cricket::VideoFormat::FpsToInterval(30) / 1E6, interval, 1);

    last_timestamp = timestamp;
  }

  // Remove stream previously added to free the external encoder instance.
  EXPECT_TRUE(channel->RemoveSendStream(kSsrc));
}

TEST_F(WebRtcVideoEngine2Test,
       ProducesIncreasingTimestampsWithResetInputSources) {
  cricket::FakeWebRtcVideoEncoderFactory encoder_factory;
  encoder_factory.AddSupportedVideoCodecType(webrtc::kVideoCodecVP8, "VP8");
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVp8Codec);

  FakeCall* fake_call = new FakeCall(webrtc::Call::Config());
  call_.reset(fake_call);
  rtc::scoped_ptr<VideoMediaChannel> channel(
      SetUpForExternalEncoderFactory(&encoder_factory, codecs));

  EXPECT_TRUE(
      channel->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc)));
  channel->SetSend(true);
  FakeVideoSendStream* stream = fake_call->GetVideoSendStreams()[0];

  FakeVideoCapturer capturer1;
  EXPECT_TRUE(channel->SetCapturer(kSsrc, &capturer1));

  cricket::CapturedFrame frame;
  frame.width = 1280;
  frame.height = 720;
  frame.fourcc = cricket::FOURCC_I420;
  frame.data_size = static_cast<uint32_t>(
      cricket::VideoFrame::SizeOf(frame.width, frame.height));
  rtc::scoped_ptr<char[]> data(new char[frame.data_size]);
  frame.data = data.get();
  memset(frame.data, 1, frame.data_size);
  const int kInitialTimestamp = 123456;
  frame.time_stamp = kInitialTimestamp;

  // Deliver initial frame.
  capturer1.SignalCapturedFrame(&frame);
  // Deliver next frame 1 second later.
  frame.time_stamp += rtc::kNumNanosecsPerSec;
  rtc::Thread::Current()->SleepMs(1000);
  capturer1.SignalCapturedFrame(&frame);

  int64_t capturer1_last_timestamp = stream->GetLastTimestamp();
  // Reset input source, should still be continuous even though input-frame
  // timestamp is less than before.
  FakeVideoCapturer capturer2;
  EXPECT_TRUE(channel->SetCapturer(kSsrc, &capturer2));

  rtc::Thread::Current()->SleepMs(1);
  // Deliver with a timestamp (10 seconds) before the previous initial one,
  // these should not be related at all anymore and it should still work fine.
  frame.time_stamp = kInitialTimestamp - 10000;
  capturer2.SignalCapturedFrame(&frame);

  // New timestamp should be at least 1ms in the future and not old.
  EXPECT_GT(stream->GetLastTimestamp(), capturer1_last_timestamp);

  EXPECT_TRUE(channel->RemoveSendStream(kSsrc));
}

VideoMediaChannel* WebRtcVideoEngine2Test::SetUpForExternalEncoderFactory(
    cricket::WebRtcVideoEncoderFactory* encoder_factory,
    const std::vector<VideoCodec>& codecs) {
  engine_.SetExternalEncoderFactory(encoder_factory);
  engine_.Init();

  VideoMediaChannel* channel =
      engine_.CreateChannel(call_.get(), cricket::VideoOptions());
  cricket::VideoSendParameters parameters;
  parameters.codecs = codecs;
  EXPECT_TRUE(channel->SetSendParameters(parameters));

  return channel;
}

VideoMediaChannel* WebRtcVideoEngine2Test::SetUpForExternalDecoderFactory(
    cricket::WebRtcVideoDecoderFactory* decoder_factory,
    const std::vector<VideoCodec>& codecs) {
  engine_.SetExternalDecoderFactory(decoder_factory);
  engine_.Init();

  VideoMediaChannel* channel =
      engine_.CreateChannel(call_.get(), cricket::VideoOptions());
  cricket::VideoRecvParameters parameters;
  parameters.codecs = codecs;
  EXPECT_TRUE(channel->SetRecvParameters(parameters));

  return channel;
}

TEST_F(WebRtcVideoEngine2Test, UsesSimulcastAdapterForVp8Factories) {
  cricket::FakeWebRtcVideoEncoderFactory encoder_factory;
  encoder_factory.AddSupportedVideoCodecType(webrtc::kVideoCodecVP8, "VP8");
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVp8Codec);

  rtc::scoped_ptr<VideoMediaChannel> channel(
      SetUpForExternalEncoderFactory(&encoder_factory, codecs));

  std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs3);

  EXPECT_TRUE(
      channel->AddSendStream(CreateSimStreamParams("cname", ssrcs)));
  EXPECT_TRUE(channel->SetSend(true));

  cricket::FakeVideoCapturer capturer;
  EXPECT_TRUE(channel->SetCapturer(ssrcs.front(), &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  EXPECT_TRUE(capturer.CaptureFrame());

  EXPECT_GT(encoder_factory.encoders().size(), 1u);

  // Verify that encoders are configured for simulcast through adapter
  // (increasing resolution and only configured to send one stream each).
  int prev_width = -1;
  for (size_t i = 0; i < encoder_factory.encoders().size(); ++i) {
    webrtc::VideoCodec codec_settings =
        encoder_factory.encoders()[i]->GetCodecSettings();
    EXPECT_EQ(0, codec_settings.numberOfSimulcastStreams);
    EXPECT_GT(codec_settings.width, prev_width);
    prev_width = codec_settings.width;
  }

  EXPECT_TRUE(channel->SetCapturer(ssrcs.front(), NULL));

  channel.reset();
  ASSERT_EQ(0u, encoder_factory.encoders().size());
}

TEST_F(WebRtcVideoEngine2Test, ChannelWithExternalH264CanChangeToInternalVp8) {
  cricket::FakeWebRtcVideoEncoderFactory encoder_factory;
  encoder_factory.AddSupportedVideoCodecType(webrtc::kVideoCodecH264, "H264");
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kH264Codec);

  rtc::scoped_ptr<VideoMediaChannel> channel(
      SetUpForExternalEncoderFactory(&encoder_factory, codecs));

  EXPECT_TRUE(
      channel->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc)));
  ASSERT_EQ(1u, encoder_factory.encoders().size());

  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(kVp8Codec);
  EXPECT_TRUE(channel->SetSendParameters(parameters));
  ASSERT_EQ(0u, encoder_factory.encoders().size());
}

TEST_F(WebRtcVideoEngine2Test,
       DontUseExternalEncoderFactoryForUnsupportedCodecs) {
  cricket::FakeWebRtcVideoEncoderFactory encoder_factory;
  encoder_factory.AddSupportedVideoCodecType(webrtc::kVideoCodecH264, "H264");
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVp8Codec);

  rtc::scoped_ptr<VideoMediaChannel> channel(
      SetUpForExternalEncoderFactory(&encoder_factory, codecs));

  EXPECT_TRUE(
      channel->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc)));
  // Make sure DestroyVideoEncoder was called on the factory.
  ASSERT_EQ(0u, encoder_factory.encoders().size());
}

TEST_F(WebRtcVideoEngine2Test,
       UsesSimulcastAdapterForVp8WithCombinedVP8AndH264Factory) {
  cricket::FakeWebRtcVideoEncoderFactory encoder_factory;
  encoder_factory.AddSupportedVideoCodecType(webrtc::kVideoCodecVP8, "VP8");
  encoder_factory.AddSupportedVideoCodecType(webrtc::kVideoCodecH264, "H264");

  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVp8Codec);

  rtc::scoped_ptr<VideoMediaChannel> channel(
      SetUpForExternalEncoderFactory(&encoder_factory, codecs));

  std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs3);

  EXPECT_TRUE(
      channel->AddSendStream(CreateSimStreamParams("cname", ssrcs)));
  EXPECT_TRUE(channel->SetSend(true));

  // Send a fake frame, or else the media engine will configure the simulcast
  // encoder adapter at a low-enough size that it'll only create a single
  // encoder layer.
  cricket::FakeVideoCapturer capturer;
  EXPECT_TRUE(channel->SetCapturer(ssrcs.front(), &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  EXPECT_TRUE(capturer.CaptureFrame());

  ASSERT_GT(encoder_factory.encoders().size(), 1u);
  EXPECT_EQ(webrtc::kVideoCodecVP8,
            encoder_factory.encoders()[0]->GetCodecSettings().codecType);

  channel.reset();
  // Make sure DestroyVideoEncoder was called on the factory.
  EXPECT_EQ(0u, encoder_factory.encoders().size());
}

TEST_F(WebRtcVideoEngine2Test,
       DestroysNonSimulcastEncoderFromCombinedVP8AndH264Factory) {
  cricket::FakeWebRtcVideoEncoderFactory encoder_factory;
  encoder_factory.AddSupportedVideoCodecType(webrtc::kVideoCodecVP8, "VP8");
  encoder_factory.AddSupportedVideoCodecType(webrtc::kVideoCodecH264, "H264");

  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kH264Codec);

  rtc::scoped_ptr<VideoMediaChannel> channel(
      SetUpForExternalEncoderFactory(&encoder_factory, codecs));

  EXPECT_TRUE(
      channel->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc)));
  ASSERT_EQ(1u, encoder_factory.encoders().size());
  EXPECT_EQ(webrtc::kVideoCodecH264,
            encoder_factory.encoders()[0]->GetCodecSettings().codecType);

  channel.reset();
  // Make sure DestroyVideoEncoder was called on the factory.
  ASSERT_EQ(0u, encoder_factory.encoders().size());
}

TEST_F(WebRtcVideoEngine2Test, SimulcastDisabledForH264) {
  cricket::FakeWebRtcVideoEncoderFactory encoder_factory;
  encoder_factory.AddSupportedVideoCodecType(webrtc::kVideoCodecH264, "H264");
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kH264Codec);

  rtc::scoped_ptr<VideoMediaChannel> channel(
      SetUpForExternalEncoderFactory(&encoder_factory, codecs));

  const std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs3);
  EXPECT_TRUE(
      channel->AddSendStream(cricket::CreateSimStreamParams("cname", ssrcs)));
  // Set the stream to 720p. This should trigger a "real" encoder
  // initialization.
  cricket::VideoFormat format(
      1280, 720, cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420);
  EXPECT_TRUE(channel->SetSendStreamFormat(ssrcs[0], format));
  ASSERT_EQ(1u, encoder_factory.encoders().size());
  FakeWebRtcVideoEncoder* encoder = encoder_factory.encoders()[0];
  EXPECT_EQ(webrtc::kVideoCodecH264, encoder->GetCodecSettings().codecType);
  EXPECT_EQ(1u, encoder->GetCodecSettings().numberOfSimulcastStreams);
}

// Test external codec with be added to the end of the supported codec list.
TEST_F(WebRtcVideoEngine2Test, ReportSupportedExternalCodecs) {
  cricket::FakeWebRtcVideoEncoderFactory encoder_factory;
  encoder_factory.AddSupportedVideoCodecType(webrtc::kVideoCodecH264, "H264");
  engine_.SetExternalEncoderFactory(&encoder_factory);
  engine_.Init();

  std::vector<cricket::VideoCodec> codecs(engine_.codecs());
  ASSERT_GE(codecs.size(), 2u);
  cricket::VideoCodec internal_codec = codecs.front();
  cricket::VideoCodec external_codec = codecs.back();

  // The external codec will appear at last.
  EXPECT_EQ("VP8", internal_codec.name);
  EXPECT_EQ("H264", external_codec.name);
}

TEST_F(WebRtcVideoEngine2Test, RegisterExternalDecodersIfSupported) {
  cricket::FakeWebRtcVideoDecoderFactory decoder_factory;
  decoder_factory.AddSupportedVideoCodecType(webrtc::kVideoCodecVP8);
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(kVp8Codec);

  rtc::scoped_ptr<VideoMediaChannel> channel(
      SetUpForExternalDecoderFactory(&decoder_factory, parameters.codecs));

  EXPECT_TRUE(
      channel->AddRecvStream(cricket::StreamParams::CreateLegacy(kSsrc)));
  ASSERT_EQ(1u, decoder_factory.decoders().size());

  // Setting codecs of the same type should not reallocate the decoder.
  EXPECT_TRUE(channel->SetRecvParameters(parameters));
  EXPECT_EQ(1, decoder_factory.GetNumCreatedDecoders());

  // Remove stream previously added to free the external decoder instance.
  EXPECT_TRUE(channel->RemoveRecvStream(kSsrc));
  EXPECT_EQ(0u, decoder_factory.decoders().size());
}

// Verifies that we can set up decoders that are not internally supported.
TEST_F(WebRtcVideoEngine2Test, RegisterExternalH264DecoderIfSupported) {
  // TODO(pbos): Do not assume that encoder/decoder support is symmetric. We
  // can't even query the WebRtcVideoDecoderFactory for supported codecs.
  // For now we add a FakeWebRtcVideoEncoderFactory to add H264 to supported
  // codecs.
  cricket::FakeWebRtcVideoEncoderFactory encoder_factory;
  encoder_factory.AddSupportedVideoCodecType(webrtc::kVideoCodecH264, "H264");
  engine_.SetExternalEncoderFactory(&encoder_factory);
  cricket::FakeWebRtcVideoDecoderFactory decoder_factory;
  decoder_factory.AddSupportedVideoCodecType(webrtc::kVideoCodecH264);
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kH264Codec);

  rtc::scoped_ptr<VideoMediaChannel> channel(
      SetUpForExternalDecoderFactory(&decoder_factory, codecs));

  EXPECT_TRUE(
      channel->AddRecvStream(cricket::StreamParams::CreateLegacy(kSsrc)));
  ASSERT_EQ(1u, decoder_factory.decoders().size());
}

class WebRtcVideoChannel2BaseTest
    : public VideoMediaChannelTest<WebRtcVideoEngine2, WebRtcVideoChannel2> {
 protected:
  typedef VideoMediaChannelTest<WebRtcVideoEngine2, WebRtcVideoChannel2> Base;

  cricket::VideoCodec DefaultCodec() override { return kVp8Codec; }
};

#define WEBRTC_BASE_TEST(test) \
  TEST_F(WebRtcVideoChannel2BaseTest, test) { Base::test(); }

#define WEBRTC_DISABLED_BASE_TEST(test) \
  TEST_F(WebRtcVideoChannel2BaseTest, DISABLED_##test) { Base::test(); }

WEBRTC_BASE_TEST(SetSend);
WEBRTC_BASE_TEST(SetSendWithoutCodecs);
WEBRTC_BASE_TEST(SetSendSetsTransportBufferSizes);

WEBRTC_BASE_TEST(GetStats);
WEBRTC_BASE_TEST(GetStatsMultipleRecvStreams);
WEBRTC_BASE_TEST(GetStatsMultipleSendStreams);

WEBRTC_BASE_TEST(SetSendBandwidth);

WEBRTC_BASE_TEST(SetSendSsrc);
WEBRTC_BASE_TEST(SetSendSsrcAfterSetCodecs);

WEBRTC_BASE_TEST(SetRenderer);
WEBRTC_BASE_TEST(AddRemoveRecvStreams);

WEBRTC_DISABLED_BASE_TEST(AddRemoveRecvStreamAndRender);

WEBRTC_BASE_TEST(AddRemoveRecvStreamsNoConference);

WEBRTC_BASE_TEST(AddRemoveSendStreams);

WEBRTC_BASE_TEST(SimulateConference);

WEBRTC_BASE_TEST(AddRemoveCapturer);

WEBRTC_BASE_TEST(RemoveCapturerWithoutAdd);

WEBRTC_BASE_TEST(AddRemoveCapturerMultipleSources);

// TODO(pbos): Figure out why this fails so often.
WEBRTC_DISABLED_BASE_TEST(HighAspectHighHeightCapturer);

WEBRTC_BASE_TEST(RejectEmptyStreamParams);

WEBRTC_BASE_TEST(AdaptResolution16x10);

WEBRTC_BASE_TEST(AdaptResolution4x3);

// TODO(juberti): Restore this test once we support sending 0 fps.
WEBRTC_DISABLED_BASE_TEST(AdaptDropAllFrames);
// TODO(juberti): Understand why we get decode errors on this test.
WEBRTC_DISABLED_BASE_TEST(AdaptFramerate);

WEBRTC_BASE_TEST(SendsLowerResolutionOnSmallerFrames);

WEBRTC_BASE_TEST(MuteStream);

WEBRTC_BASE_TEST(MultipleSendStreams);

WEBRTC_BASE_TEST(SetSendStreamFormat0x0);

// TODO(zhurunz): Fix the flakey test.
WEBRTC_DISABLED_BASE_TEST(SetSendStreamFormat);

TEST_F(WebRtcVideoChannel2BaseTest, SendAndReceiveVp8Vga) {
  SendAndReceive(cricket::VideoCodec(100, "VP8", 640, 400, 30, 0));
}

TEST_F(WebRtcVideoChannel2BaseTest, SendAndReceiveVp8Qvga) {
  SendAndReceive(cricket::VideoCodec(100, "VP8", 320, 200, 30, 0));
}

TEST_F(WebRtcVideoChannel2BaseTest, SendAndReceiveVp8SvcQqvga) {
  SendAndReceive(cricket::VideoCodec(100, "VP8", 160, 100, 30, 0));
}

TEST_F(WebRtcVideoChannel2BaseTest, TwoStreamsSendAndReceive) {
  Base::TwoStreamsSendAndReceive(kVp8Codec);
}

TEST_F(WebRtcVideoChannel2BaseTest, TwoStreamsReUseFirstStream) {
  Base::TwoStreamsReUseFirstStream(kVp8Codec);
}

//Disabled for TSan: https://bugs.chromium.org/p/webrtc/issues/detail?id=4963
#if !defined(THREAD_SANITIZER)
WEBRTC_BASE_TEST(SendManyResizeOnce);
#endif  // THREAD_SANITIZER

// TODO(pbos): Enable and figure out why this fails (or should work).
TEST_F(WebRtcVideoChannel2BaseTest, DISABLED_SendVp8HdAndReceiveAdaptedVp8Vga) {
  EXPECT_TRUE(channel_->SetCapturer(kSsrc, NULL));
  EXPECT_TRUE(channel_->SetRenderer(kDefaultReceiveSsrc, &renderer_));
  channel_->UpdateAspectRatio(1280, 720);
  video_capturer_.reset(new cricket::FakeVideoCapturer);
  const std::vector<cricket::VideoFormat>* formats =
      video_capturer_->GetSupportedFormats();
  cricket::VideoFormat capture_format_hd = (*formats)[0];
  EXPECT_EQ(cricket::CS_RUNNING, video_capturer_->Start(capture_format_hd));
  EXPECT_TRUE(channel_->SetCapturer(kSsrc, video_capturer_.get()));

  // Capture format HD -> adapt (OnOutputFormatRequest VGA) -> VGA.
  cricket::VideoCodec codec = kVp8Codec720p;
  EXPECT_TRUE(SetOneCodec(codec));
  codec.width /= 2;
  codec.height /= 2;
  EXPECT_TRUE(SetSend(true));
  EXPECT_EQ(0, renderer_.num_rendered_frames());
  EXPECT_TRUE(SendFrame());
  EXPECT_FRAME_WAIT(1, codec.width, codec.height, kTimeout);
}

class WebRtcVideoChannel2Test : public WebRtcVideoEngine2Test {
 public:
  WebRtcVideoChannel2Test() : WebRtcVideoChannel2Test("") {}
  explicit WebRtcVideoChannel2Test(const char* field_trials)
      : WebRtcVideoEngine2Test(field_trials), last_ssrc_(0) {}
  void SetUp() override {
    fake_call_.reset(new FakeCall(webrtc::Call::Config()));
    engine_.Init();
    channel_.reset(
        engine_.CreateChannel(fake_call_.get(), cricket::VideoOptions()));
    last_ssrc_ = 123;
    send_parameters_.codecs = engine_.codecs();
    recv_parameters_.codecs = engine_.codecs();
    ASSERT_TRUE(channel_->SetSendParameters(send_parameters_));
  }

 protected:
  FakeVideoSendStream* AddSendStream() {
    return AddSendStream(StreamParams::CreateLegacy(++last_ssrc_));
  }

  FakeVideoSendStream* AddSendStream(const StreamParams& sp) {
    size_t num_streams = fake_call_->GetVideoSendStreams().size();
    EXPECT_TRUE(channel_->AddSendStream(sp));
    std::vector<FakeVideoSendStream*> streams =
        fake_call_->GetVideoSendStreams();
    EXPECT_EQ(num_streams + 1, streams.size());
    return streams[streams.size() - 1];
  }

  std::vector<FakeVideoSendStream*> GetFakeSendStreams() {
    return fake_call_->GetVideoSendStreams();
  }

  FakeVideoReceiveStream* AddRecvStream() {
    return AddRecvStream(StreamParams::CreateLegacy(++last_ssrc_));
  }

  FakeVideoReceiveStream* AddRecvStream(const StreamParams& sp) {
    size_t num_streams = fake_call_->GetVideoReceiveStreams().size();
    EXPECT_TRUE(channel_->AddRecvStream(sp));
    std::vector<FakeVideoReceiveStream*> streams =
        fake_call_->GetVideoReceiveStreams();
    EXPECT_EQ(num_streams + 1, streams.size());
    return streams[streams.size() - 1];
  }

  void SetSendCodecsShouldWorkForBitrates(const char* min_bitrate_kbps,
                                          int expected_min_bitrate_bps,
                                          const char* start_bitrate_kbps,
                                          int expected_start_bitrate_bps,
                                          const char* max_bitrate_kbps,
                                          int expected_max_bitrate_bps) {
    auto& codecs = send_parameters_.codecs;
    codecs.clear();
    codecs.push_back(kVp8Codec);
    codecs[0].params[kCodecParamMinBitrate] = min_bitrate_kbps;
    codecs[0].params[kCodecParamStartBitrate] = start_bitrate_kbps;
    codecs[0].params[kCodecParamMaxBitrate] = max_bitrate_kbps;
    EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

    EXPECT_EQ(expected_min_bitrate_bps,
              fake_call_->GetConfig().bitrate_config.min_bitrate_bps);
    EXPECT_EQ(expected_start_bitrate_bps,
              fake_call_->GetConfig().bitrate_config.start_bitrate_bps);
    EXPECT_EQ(expected_max_bitrate_bps,
              fake_call_->GetConfig().bitrate_config.max_bitrate_bps);
  }

  void TestSetSendRtpHeaderExtensions(const std::string& cricket_ext,
                                      const std::string& webrtc_ext) {
    // Enable extension.
    const int id = 1;
    cricket::VideoSendParameters parameters = send_parameters_;
    parameters.extensions.push_back(
        cricket::RtpHeaderExtension(cricket_ext, id));
    EXPECT_TRUE(channel_->SetSendParameters(parameters));
    FakeVideoSendStream* send_stream =
        AddSendStream(cricket::StreamParams::CreateLegacy(123));

    // Verify the send extension id.
    ASSERT_EQ(1u, send_stream->GetConfig().rtp.extensions.size());
    EXPECT_EQ(id, send_stream->GetConfig().rtp.extensions[0].id);
    EXPECT_EQ(webrtc_ext, send_stream->GetConfig().rtp.extensions[0].name);
    // Verify call with same set of extensions returns true.
    EXPECT_TRUE(channel_->SetSendParameters(parameters));
    // Verify that SetSendRtpHeaderExtensions doesn't implicitly add them for
    // receivers.
    EXPECT_TRUE(AddRecvStream(cricket::StreamParams::CreateLegacy(123))
                    ->GetConfig()
                    .rtp.extensions.empty());

    // Verify that existing RTP header extensions can be removed.
    EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
    ASSERT_EQ(1u, fake_call_->GetVideoSendStreams().size());
    send_stream = fake_call_->GetVideoSendStreams()[0];
    EXPECT_TRUE(send_stream->GetConfig().rtp.extensions.empty());

    // Verify that adding receive RTP header extensions adds them for existing
    // streams.
    EXPECT_TRUE(channel_->SetSendParameters(parameters));
    send_stream = fake_call_->GetVideoSendStreams()[0];
    ASSERT_EQ(1u, send_stream->GetConfig().rtp.extensions.size());
    EXPECT_EQ(id, send_stream->GetConfig().rtp.extensions[0].id);
    EXPECT_EQ(webrtc_ext, send_stream->GetConfig().rtp.extensions[0].name);
  }

  void TestSetRecvRtpHeaderExtensions(const std::string& cricket_ext,
                                      const std::string& webrtc_ext) {
    // Enable extension.
    const int id = 1;
    cricket::VideoRecvParameters parameters = recv_parameters_;
    parameters.extensions.push_back(
        cricket::RtpHeaderExtension(cricket_ext, id));
    EXPECT_TRUE(channel_->SetRecvParameters(parameters));

    FakeVideoReceiveStream* recv_stream =
        AddRecvStream(cricket::StreamParams::CreateLegacy(123));

    // Verify the recv extension id.
    ASSERT_EQ(1u, recv_stream->GetConfig().rtp.extensions.size());
    EXPECT_EQ(id, recv_stream->GetConfig().rtp.extensions[0].id);
    EXPECT_EQ(webrtc_ext, recv_stream->GetConfig().rtp.extensions[0].name);
    // Verify call with same set of extensions returns true.
    EXPECT_TRUE(channel_->SetRecvParameters(parameters));

    // Verify that SetRecvRtpHeaderExtensions doesn't implicitly add them for
    // senders.
    EXPECT_TRUE(AddSendStream(cricket::StreamParams::CreateLegacy(123))
                    ->GetConfig()
                    .rtp.extensions.empty());

    // Verify that existing RTP header extensions can be removed.
    EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));
    ASSERT_EQ(1u, fake_call_->GetVideoReceiveStreams().size());
    recv_stream = fake_call_->GetVideoReceiveStreams()[0];
    EXPECT_TRUE(recv_stream->GetConfig().rtp.extensions.empty());

    // Verify that adding receive RTP header extensions adds them for existing
    // streams.
    EXPECT_TRUE(channel_->SetRecvParameters(parameters));
    recv_stream = fake_call_->GetVideoReceiveStreams()[0];
    ASSERT_EQ(1u, recv_stream->GetConfig().rtp.extensions.size());
    EXPECT_EQ(id, recv_stream->GetConfig().rtp.extensions[0].id);
    EXPECT_EQ(webrtc_ext, recv_stream->GetConfig().rtp.extensions[0].name);
  }

  void TestExtensionFilter(const std::vector<std::string>& extensions,
                           const std::string& expected_extension) {
    cricket::VideoSendParameters parameters = send_parameters_;
    int expected_id = -1;
    int id = 1;
    for (const std::string& extension : extensions) {
      if (extension == expected_extension)
        expected_id = id;
      parameters.extensions.push_back(
          cricket::RtpHeaderExtension(extension, id++));
    }
    EXPECT_TRUE(channel_->SetSendParameters(parameters));
    FakeVideoSendStream* send_stream =
        AddSendStream(cricket::StreamParams::CreateLegacy(123));

    // Verify that only one of them has been set, and that it is the one with
    // highest priority (transport sequence number).
    ASSERT_EQ(1u, send_stream->GetConfig().rtp.extensions.size());
    EXPECT_EQ(expected_id, send_stream->GetConfig().rtp.extensions[0].id);
    EXPECT_EQ(expected_extension,
              send_stream->GetConfig().rtp.extensions[0].name);
  }

  void TestCpuAdaptation(bool enable_overuse, bool is_screenshare);
  void TestReceiverLocalSsrcConfiguration(bool receiver_first);
  void TestReceiveUnsignalledSsrcPacket(uint8_t payload_type,
                                        bool expect_created_receive_stream);

  FakeVideoSendStream* SetDenoisingOption(
      const cricket::VideoSendParameters& parameters, bool enabled) {
    cricket::VideoSendParameters params = parameters;
    params.options.video_noise_reduction = rtc::Optional<bool>(enabled);
    channel_->SetSendParameters(params);
    return fake_call_->GetVideoSendStreams().back();
  }

  FakeVideoSendStream* SetUpSimulcast(bool enabled, bool with_rtx) {
    const int kRtxSsrcOffset = 0xDEADBEEF;
    last_ssrc_ += 3;
    std::vector<uint32_t> ssrcs;
    std::vector<uint32_t> rtx_ssrcs;
    uint32_t num_streams = enabled ? 3 : 1;
    for (uint32_t i = 0; i < num_streams; ++i) {
      uint32_t ssrc = last_ssrc_ + i;
      ssrcs.push_back(ssrc);
      if (with_rtx) {
        rtx_ssrcs.push_back(ssrc + kRtxSsrcOffset);
      }
    }
    if (with_rtx) {
      return AddSendStream(
          cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs));
    }
    return AddSendStream(CreateSimStreamParams("cname", ssrcs));
  }

  rtc::scoped_ptr<FakeCall> fake_call_;
  rtc::scoped_ptr<VideoMediaChannel> channel_;
  cricket::VideoSendParameters send_parameters_;
  cricket::VideoRecvParameters recv_parameters_;
  uint32_t last_ssrc_;
};

TEST_F(WebRtcVideoChannel2Test, SetsSyncGroupFromSyncLabel) {
  const uint32_t kVideoSsrc = 123;
  const std::string kSyncLabel = "AvSyncLabel";

  cricket::StreamParams sp = cricket::StreamParams::CreateLegacy(kVideoSsrc);
  sp.sync_label = kSyncLabel;
  EXPECT_TRUE(channel_->AddRecvStream(sp));

  EXPECT_EQ(1, fake_call_->GetVideoReceiveStreams().size());
  EXPECT_EQ(kSyncLabel,
            fake_call_->GetVideoReceiveStreams()[0]->GetConfig().sync_group)
      << "SyncGroup should be set based on sync_label";
}

TEST_F(WebRtcVideoChannel2Test, RecvStreamWithSimAndRtx) {
  cricket::VideoSendParameters parameters;
  parameters.codecs = engine_.codecs();
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_TRUE(channel_->SetSend(true));
  parameters.options.conference_mode = rtc::Optional<bool>(true);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));

  // Send side.
  const std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs1);
  const std::vector<uint32_t> rtx_ssrcs = MAKE_VECTOR(kRtxSsrcs1);
  FakeVideoSendStream* send_stream = AddSendStream(
      cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs));

  ASSERT_EQ(rtx_ssrcs.size(), send_stream->GetConfig().rtp.rtx.ssrcs.size());
  for (size_t i = 0; i < rtx_ssrcs.size(); ++i)
    EXPECT_EQ(rtx_ssrcs[i], send_stream->GetConfig().rtp.rtx.ssrcs[i]);

  // Receiver side.
  FakeVideoReceiveStream* recv_stream = AddRecvStream(
      cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs));
  ASSERT_GT(recv_stream->GetConfig().rtp.rtx.size(), 0u)
      << "No SSRCs for RTX configured by AddRecvStream.";
  ASSERT_EQ(1u, recv_stream->GetConfig().rtp.rtx.size())
      << "This test only works with one receive codec. Please update the test.";
  EXPECT_EQ(rtx_ssrcs[0],
            recv_stream->GetConfig().rtp.rtx.begin()->second.ssrc);
  // TODO(pbos): Make sure we set the RTX for correct payloads etc.
}

TEST_F(WebRtcVideoChannel2Test, RecvStreamWithRtx) {
  // Setup one channel with an associated RTX stream.
  cricket::StreamParams params =
      cricket::StreamParams::CreateLegacy(kSsrcs1[0]);
  params.AddFidSsrc(kSsrcs1[0], kRtxSsrcs1[0]);
  FakeVideoReceiveStream* recv_stream = AddRecvStream(params);
  ASSERT_EQ(1u, recv_stream->GetConfig().rtp.rtx.size());
  EXPECT_EQ(kRtxSsrcs1[0],
            recv_stream->GetConfig().rtp.rtx.begin()->second.ssrc);
}

TEST_F(WebRtcVideoChannel2Test, RecvStreamNoRtx) {
  // Setup one channel without an associated RTX stream.
  cricket::StreamParams params =
      cricket::StreamParams::CreateLegacy(kSsrcs1[0]);
  FakeVideoReceiveStream* recv_stream = AddRecvStream(params);
  ASSERT_TRUE(recv_stream->GetConfig().rtp.rtx.empty());
}

TEST_F(WebRtcVideoChannel2Test, NoHeaderExtesionsByDefault) {
  FakeVideoSendStream* send_stream =
      AddSendStream(cricket::StreamParams::CreateLegacy(kSsrcs1[0]));
  ASSERT_TRUE(send_stream->GetConfig().rtp.extensions.empty());

  FakeVideoReceiveStream* recv_stream =
      AddRecvStream(cricket::StreamParams::CreateLegacy(kSsrcs1[0]));
  ASSERT_TRUE(recv_stream->GetConfig().rtp.extensions.empty());
}

// Test support for RTP timestamp offset header extension.
TEST_F(WebRtcVideoChannel2Test, SendRtpTimestampOffsetHeaderExtensions) {
  TestSetSendRtpHeaderExtensions(kRtpTimestampOffsetHeaderExtension,
                                 webrtc::RtpExtension::kTOffset);
}
TEST_F(WebRtcVideoChannel2Test, RecvRtpTimestampOffsetHeaderExtensions) {
  TestSetRecvRtpHeaderExtensions(kRtpTimestampOffsetHeaderExtension,
                                 webrtc::RtpExtension::kTOffset);
}

// Test support for absolute send time header extension.
TEST_F(WebRtcVideoChannel2Test, SendAbsoluteSendTimeHeaderExtensions) {
  TestSetSendRtpHeaderExtensions(kRtpAbsoluteSenderTimeHeaderExtension,
                                 webrtc::RtpExtension::kAbsSendTime);
}
TEST_F(WebRtcVideoChannel2Test, RecvAbsoluteSendTimeHeaderExtensions) {
  TestSetRecvRtpHeaderExtensions(kRtpAbsoluteSenderTimeHeaderExtension,
                                 webrtc::RtpExtension::kAbsSendTime);
}

TEST_F(WebRtcVideoChannel2Test, FiltersExtensionsPicksTransportSeqNum) {
  // Enable three redundant extensions.
  std::vector<std::string> extensions;
  extensions.push_back(kRtpAbsoluteSenderTimeHeaderExtension);
  extensions.push_back(kRtpTimestampOffsetHeaderExtension);
  extensions.push_back(kRtpTransportSequenceNumberHeaderExtension);
  TestExtensionFilter(extensions, kRtpTransportSequenceNumberHeaderExtension);
}

TEST_F(WebRtcVideoChannel2Test, FiltersExtensionsPicksAbsSendTime) {
  // Enable two redundant extensions.
  std::vector<std::string> extensions;
  extensions.push_back(kRtpAbsoluteSenderTimeHeaderExtension);
  extensions.push_back(kRtpTimestampOffsetHeaderExtension);
  TestExtensionFilter(extensions, kRtpAbsoluteSenderTimeHeaderExtension);
}

class WebRtcVideoChannel2WithSendSideBweTest : public WebRtcVideoChannel2Test {
 public:
  WebRtcVideoChannel2WithSendSideBweTest()
      : WebRtcVideoChannel2Test("WebRTC-SendSideBwe/Enabled/") {}
};

// Test support for transport sequence number header extension.
TEST_F(WebRtcVideoChannel2WithSendSideBweTest,
       SendTransportSequenceNumberHeaderExtensions) {
  TestSetSendRtpHeaderExtensions(
      kRtpTransportSequenceNumberHeaderExtension,
      webrtc::RtpExtension::kTransportSequenceNumber);
}
TEST_F(WebRtcVideoChannel2WithSendSideBweTest,
       RecvTransportSequenceNumberHeaderExtensions) {
  TestSetRecvRtpHeaderExtensions(
      kRtpTransportSequenceNumberHeaderExtension,
      webrtc::RtpExtension::kTransportSequenceNumber);
}

// Test support for video rotation header extension.
TEST_F(WebRtcVideoChannel2Test, SendVideoRotationHeaderExtensions) {
  TestSetSendRtpHeaderExtensions(kRtpVideoRotationHeaderExtension,
                                 webrtc::RtpExtension::kVideoRotation);
}
TEST_F(WebRtcVideoChannel2Test, RecvVideoRotationHeaderExtensions) {
  TestSetRecvRtpHeaderExtensions(kRtpVideoRotationHeaderExtension,
                                 webrtc::RtpExtension::kVideoRotation);
}

TEST_F(WebRtcVideoChannel2Test, IdenticalSendExtensionsDoesntRecreateStream) {
  const int kAbsSendTimeId = 1;
  const int kVideoRotationId = 2;
  send_parameters_.extensions.push_back(cricket::RtpHeaderExtension(
      kRtpAbsoluteSenderTimeHeaderExtension, kAbsSendTimeId));
  send_parameters_.extensions.push_back(cricket::RtpHeaderExtension(
      kRtpVideoRotationHeaderExtension, kVideoRotationId));

  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  FakeVideoSendStream* send_stream =
      AddSendStream(cricket::StreamParams::CreateLegacy(123));

  EXPECT_EQ(1, fake_call_->GetNumCreatedSendStreams());
  ASSERT_EQ(2u, send_stream->GetConfig().rtp.extensions.size());

  // Setting the same extensions (even if in different order) shouldn't
  // reallocate the stream.
  std::reverse(send_parameters_.extensions.begin(),
               send_parameters_.extensions.end());
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  EXPECT_EQ(1, fake_call_->GetNumCreatedSendStreams());

  // Setting different extensions should recreate the stream.
  send_parameters_.extensions.resize(1);
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  EXPECT_EQ(2, fake_call_->GetNumCreatedSendStreams());
}

TEST_F(WebRtcVideoChannel2Test, IdenticalRecvExtensionsDoesntRecreateStream) {
  const int kTOffsetId = 1;
  const int kAbsSendTimeId = 2;
  const int kVideoRotationId = 3;
  recv_parameters_.extensions.push_back(cricket::RtpHeaderExtension(
      kRtpAbsoluteSenderTimeHeaderExtension, kAbsSendTimeId));
  recv_parameters_.extensions.push_back(cricket::RtpHeaderExtension(
      kRtpTimestampOffsetHeaderExtension, kTOffsetId));
  recv_parameters_.extensions.push_back(cricket::RtpHeaderExtension(
      kRtpVideoRotationHeaderExtension, kVideoRotationId));

  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));
  FakeVideoReceiveStream* recv_stream =
      AddRecvStream(cricket::StreamParams::CreateLegacy(123));

  EXPECT_EQ(1, fake_call_->GetNumCreatedReceiveStreams());
  ASSERT_EQ(3u, recv_stream->GetConfig().rtp.extensions.size());

  // Setting the same extensions (even if in different order) shouldn't
  // reallocate the stream.
  std::reverse(recv_parameters_.extensions.begin(),
               recv_parameters_.extensions.end());
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));

  EXPECT_EQ(1, fake_call_->GetNumCreatedReceiveStreams());

  // Setting different extensions should recreate the stream.
  recv_parameters_.extensions.resize(1);
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));

  EXPECT_EQ(2, fake_call_->GetNumCreatedReceiveStreams());
}

TEST_F(WebRtcVideoChannel2Test,
       SetSendRtpHeaderExtensionsExcludeUnsupportedExtensions) {
  const int kUnsupportedId = 1;
  const int kTOffsetId = 2;

  send_parameters_.extensions.push_back(
      cricket::RtpHeaderExtension(kUnsupportedExtensionName, kUnsupportedId));
  send_parameters_.extensions.push_back(
      cricket::RtpHeaderExtension(webrtc::RtpExtension::kTOffset, kTOffsetId));
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  FakeVideoSendStream* send_stream =
      AddSendStream(cricket::StreamParams::CreateLegacy(123));

  // Only timestamp offset extension is set to send stream,
  // unsupported rtp extension is ignored.
  ASSERT_EQ(1u, send_stream->GetConfig().rtp.extensions.size());
  EXPECT_STREQ(webrtc::RtpExtension::kTOffset,
               send_stream->GetConfig().rtp.extensions[0].name.c_str());
}

TEST_F(WebRtcVideoChannel2Test,
       SetRecvRtpHeaderExtensionsExcludeUnsupportedExtensions) {
  const int kUnsupportedId = 1;
  const int kTOffsetId = 2;

  recv_parameters_.extensions.push_back(
      cricket::RtpHeaderExtension(kUnsupportedExtensionName, kUnsupportedId));
  recv_parameters_.extensions.push_back(
      cricket::RtpHeaderExtension(webrtc::RtpExtension::kTOffset, kTOffsetId));
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));
  FakeVideoReceiveStream* recv_stream =
      AddRecvStream(cricket::StreamParams::CreateLegacy(123));

  // Only timestamp offset extension is set to receive stream,
  // unsupported rtp extension is ignored.
  ASSERT_EQ(1u, recv_stream->GetConfig().rtp.extensions.size());
  EXPECT_STREQ(webrtc::RtpExtension::kTOffset,
               recv_stream->GetConfig().rtp.extensions[0].name.c_str());
}

TEST_F(WebRtcVideoChannel2Test, SetSendRtpHeaderExtensionsRejectsIncorrectIds) {
  const int kIncorrectIds[] = {-2, -1, 0, 15, 16};
  for (size_t i = 0; i < arraysize(kIncorrectIds); ++i) {
    send_parameters_.extensions.push_back(cricket::RtpHeaderExtension(
        webrtc::RtpExtension::kTOffset, kIncorrectIds[i]));
    EXPECT_FALSE(channel_->SetSendParameters(send_parameters_))
        << "Bad extension id '" << kIncorrectIds[i] << "' accepted.";
  }
}

TEST_F(WebRtcVideoChannel2Test, SetRecvRtpHeaderExtensionsRejectsIncorrectIds) {
  const int kIncorrectIds[] = {-2, -1, 0, 15, 16};
  for (size_t i = 0; i < arraysize(kIncorrectIds); ++i) {
    recv_parameters_.extensions.push_back(cricket::RtpHeaderExtension(
        webrtc::RtpExtension::kTOffset, kIncorrectIds[i]));
    EXPECT_FALSE(channel_->SetRecvParameters(recv_parameters_))
        << "Bad extension id '" << kIncorrectIds[i] << "' accepted.";
  }
}

TEST_F(WebRtcVideoChannel2Test, SetSendRtpHeaderExtensionsRejectsDuplicateIds) {
  const int id = 1;
  send_parameters_.extensions.push_back(
      cricket::RtpHeaderExtension(webrtc::RtpExtension::kTOffset, id));
  send_parameters_.extensions.push_back(
      cricket::RtpHeaderExtension(kRtpAbsoluteSenderTimeHeaderExtension, id));
  EXPECT_FALSE(channel_->SetSendParameters(send_parameters_));

  // Duplicate entries are also not supported.
  send_parameters_.extensions.clear();
  send_parameters_.extensions.push_back(
      cricket::RtpHeaderExtension(webrtc::RtpExtension::kTOffset, id));
  send_parameters_.extensions.push_back(send_parameters_.extensions.back());
  EXPECT_FALSE(channel_->SetSendParameters(send_parameters_));
}

TEST_F(WebRtcVideoChannel2Test, SetRecvRtpHeaderExtensionsRejectsDuplicateIds) {
  const int id = 1;
  recv_parameters_.extensions.push_back(
      cricket::RtpHeaderExtension(webrtc::RtpExtension::kTOffset, id));
  recv_parameters_.extensions.push_back(
      cricket::RtpHeaderExtension(kRtpAbsoluteSenderTimeHeaderExtension, id));
  EXPECT_FALSE(channel_->SetRecvParameters(recv_parameters_));

  // Duplicate entries are also not supported.
  recv_parameters_.extensions.clear();
  recv_parameters_.extensions.push_back(
      cricket::RtpHeaderExtension(webrtc::RtpExtension::kTOffset, id));
  recv_parameters_.extensions.push_back(recv_parameters_.extensions.back());
  EXPECT_FALSE(channel_->SetRecvParameters(recv_parameters_));
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_LeakyBucketTest) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_BufferedModeLatency) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_AdditiveVideoOptions) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, AddRecvStreamOnlyUsesOneReceiveStream) {
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  EXPECT_EQ(1u, fake_call_->GetVideoReceiveStreams().size());
}

TEST_F(WebRtcVideoChannel2Test, RtcpIsCompoundByDefault) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  EXPECT_EQ(webrtc::RtcpMode::kCompound, stream->GetConfig().rtp.rtcp_mode);
}

TEST_F(WebRtcVideoChannel2Test, RembIsEnabledByDefault) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  EXPECT_TRUE(stream->GetConfig().rtp.remb);
}

TEST_F(WebRtcVideoChannel2Test, TransportCcIsEnabledByDefault) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  EXPECT_TRUE(stream->GetConfig().rtp.transport_cc);
}

TEST_F(WebRtcVideoChannel2Test, RembCanBeEnabledAndDisabled) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  EXPECT_TRUE(stream->GetConfig().rtp.remb);

  // Verify that REMB is turned off when send(!) codecs without REMB are set.
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(kVp8Codec);
  EXPECT_TRUE(parameters.codecs[0].feedback_params.params().empty());
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_FALSE(stream->GetConfig().rtp.remb);

  // Verify that REMB is turned on when setting default codecs since the
  // default codecs have REMB enabled.
  parameters.codecs = engine_.codecs();
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_TRUE(stream->GetConfig().rtp.remb);
}

TEST_F(WebRtcVideoChannel2Test, TransportCcCanBeEnabledAndDisabled) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  EXPECT_TRUE(stream->GetConfig().rtp.transport_cc);

  // Verify that transport cc feedback is turned off when send(!) codecs without
  // transport cc feedback are set.
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(kVp8Codec);
  EXPECT_TRUE(parameters.codecs[0].feedback_params.params().empty());
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_FALSE(stream->GetConfig().rtp.transport_cc);

  // Verify that transport cc feedback is turned on when setting default codecs
  // since the default codecs have transport cc feedback enabled.
  parameters.codecs = engine_.codecs();
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_TRUE(stream->GetConfig().rtp.transport_cc);
}

TEST_F(WebRtcVideoChannel2Test, NackIsEnabledByDefault) {
  VerifyCodecHasDefaultFeedbackParams(default_codec_);

  cricket::VideoSendParameters parameters;
  parameters.codecs = engine_.codecs();
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_TRUE(channel_->SetSend(true));

  // Send side.
  FakeVideoSendStream* send_stream =
      AddSendStream(cricket::StreamParams::CreateLegacy(1));
  EXPECT_GT(send_stream->GetConfig().rtp.nack.rtp_history_ms, 0);

  // Receiver side.
  FakeVideoReceiveStream* recv_stream =
      AddRecvStream(cricket::StreamParams::CreateLegacy(1));
  EXPECT_GT(recv_stream->GetConfig().rtp.nack.rtp_history_ms, 0);

  // Nack history size should match between sender and receiver.
  EXPECT_EQ(send_stream->GetConfig().rtp.nack.rtp_history_ms,
            recv_stream->GetConfig().rtp.nack.rtp_history_ms);
}

TEST_F(WebRtcVideoChannel2Test, NackCanBeEnabledAndDisabled) {
  FakeVideoSendStream* send_stream = AddSendStream();
  FakeVideoReceiveStream* recv_stream = AddRecvStream();

  EXPECT_GT(recv_stream->GetConfig().rtp.nack.rtp_history_ms, 0);
  EXPECT_GT(send_stream->GetConfig().rtp.nack.rtp_history_ms, 0);

  // Verify that NACK is turned off when send(!) codecs without NACK are set.
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(kVp8Codec);
  EXPECT_TRUE(parameters.codecs[0].feedback_params.params().empty());
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  recv_stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_EQ(0, recv_stream->GetConfig().rtp.nack.rtp_history_ms);
  send_stream = fake_call_->GetVideoSendStreams()[0];
  EXPECT_EQ(0, send_stream->GetConfig().rtp.nack.rtp_history_ms);

  // Verify that NACK is turned on when setting default codecs since the
  // default codecs have NACK enabled.
  parameters.codecs = engine_.codecs();
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  recv_stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_GT(recv_stream->GetConfig().rtp.nack.rtp_history_ms, 0);
  send_stream = fake_call_->GetVideoSendStreams()[0];
  EXPECT_GT(send_stream->GetConfig().rtp.nack.rtp_history_ms, 0);
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_VideoProtectionInterop) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_VideoProtectionInteropReversed) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_HybridNackFecConference) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_AddRemoveRecvStreamConference) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_SetBandwidthAuto) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_SetBandwidthAutoCapped) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_SetBandwidthFixed) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_SetBandwidthInConference) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, UsesCorrectSettingsForScreencast) {
  static const int kScreenshareMinBitrateKbps = 800;
  cricket::VideoCodec codec = kVp8Codec360p;
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(codec);
  parameters.options.screencast_min_bitrate =
      rtc::Optional<int>(kScreenshareMinBitrateKbps);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));

  AddSendStream();

  cricket::FakeVideoCapturer capturer;
  capturer.SetScreencast(false);
  EXPECT_TRUE(channel_->SetCapturer(last_ssrc_, &capturer));
  cricket::VideoFormat capture_format_hd =
      capturer.GetSupportedFormats()->front();
  EXPECT_EQ(1280, capture_format_hd.width);
  EXPECT_EQ(720, capture_format_hd.height);
  EXPECT_EQ(cricket::CS_RUNNING, capturer.Start(capture_format_hd));

  EXPECT_TRUE(channel_->SetSend(true));

  EXPECT_TRUE(capturer.CaptureFrame());
  ASSERT_EQ(1u, fake_call_->GetVideoSendStreams().size());
  FakeVideoSendStream* send_stream = fake_call_->GetVideoSendStreams().front();

  EXPECT_EQ(1, send_stream->GetNumberOfSwappedFrames());

  // Verify non-screencast settings.
  webrtc::VideoEncoderConfig encoder_config = send_stream->GetEncoderConfig();
  EXPECT_EQ(webrtc::VideoEncoderConfig::ContentType::kRealtimeVideo,
            encoder_config.content_type);
  EXPECT_EQ(codec.width, encoder_config.streams.front().width);
  EXPECT_EQ(codec.height, encoder_config.streams.front().height);
  EXPECT_EQ(0, encoder_config.min_transmit_bitrate_bps)
      << "Non-screenshare shouldn't use min-transmit bitrate.";

  capturer.SetScreencast(true);
  EXPECT_TRUE(capturer.CaptureFrame());

  EXPECT_EQ(2, send_stream->GetNumberOfSwappedFrames());

  // Verify screencast settings.
  encoder_config = send_stream->GetEncoderConfig();
  EXPECT_EQ(webrtc::VideoEncoderConfig::ContentType::kScreen,
            encoder_config.content_type);
  EXPECT_EQ(kScreenshareMinBitrateKbps * 1000,
            encoder_config.min_transmit_bitrate_bps);

  EXPECT_EQ(capture_format_hd.width, encoder_config.streams.front().width);
  EXPECT_EQ(capture_format_hd.height, encoder_config.streams.front().height);
  EXPECT_TRUE(encoder_config.streams[0].temporal_layer_thresholds_bps.empty());

  EXPECT_TRUE(channel_->SetCapturer(last_ssrc_, NULL));
}

TEST_F(WebRtcVideoChannel2Test,
       ConferenceModeScreencastConfiguresTemporalLayer) {
  static const int kConferenceScreencastTemporalBitrateBps =
      ScreenshareLayerConfig::GetDefault().tl0_bitrate_kbps * 1000;
  send_parameters_.options.conference_mode = rtc::Optional<bool>(true);
  channel_->SetSendParameters(send_parameters_);

  AddSendStream();

  cricket::FakeVideoCapturer capturer;
  capturer.SetScreencast(true);
  EXPECT_TRUE(channel_->SetCapturer(last_ssrc_, &capturer));
  cricket::VideoFormat capture_format_hd =
      capturer.GetSupportedFormats()->front();
  EXPECT_EQ(cricket::CS_RUNNING, capturer.Start(capture_format_hd));

  EXPECT_TRUE(channel_->SetSend(true));

  EXPECT_TRUE(capturer.CaptureFrame());
  ASSERT_EQ(1u, fake_call_->GetVideoSendStreams().size());
  FakeVideoSendStream* send_stream = fake_call_->GetVideoSendStreams().front();

  webrtc::VideoEncoderConfig encoder_config = send_stream->GetEncoderConfig();

  // Verify screencast settings.
  encoder_config = send_stream->GetEncoderConfig();
  EXPECT_EQ(webrtc::VideoEncoderConfig::ContentType::kScreen,
            encoder_config.content_type);
  ASSERT_EQ(1u, encoder_config.streams.size());
  ASSERT_EQ(1u, encoder_config.streams[0].temporal_layer_thresholds_bps.size());
  EXPECT_EQ(kConferenceScreencastTemporalBitrateBps,
            encoder_config.streams[0].temporal_layer_thresholds_bps[0]);

  EXPECT_TRUE(channel_->SetCapturer(last_ssrc_, NULL));
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_SetSendSsrcAndCname) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test,
       DISABLED_SetSendSsrcAfterCreatingReceiveChannel) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, SuspendBelowMinBitrateDisabledByDefault) {
  FakeVideoSendStream* stream = AddSendStream();
  EXPECT_FALSE(stream->GetConfig().suspend_below_min_bitrate);
}

TEST_F(WebRtcVideoChannel2Test, SetOptionsWithSuspendBelowMinBitrate) {
  send_parameters_.options.suspend_below_min_bitrate =
      rtc::Optional<bool>(true);
  channel_->SetSendParameters(send_parameters_);

  FakeVideoSendStream* stream = AddSendStream();
  EXPECT_TRUE(stream->GetConfig().suspend_below_min_bitrate);

  send_parameters_.options.suspend_below_min_bitrate =
      rtc::Optional<bool>(false);
  channel_->SetSendParameters(send_parameters_);

  stream = fake_call_->GetVideoSendStreams()[0];
  EXPECT_FALSE(stream->GetConfig().suspend_below_min_bitrate);
}

TEST_F(WebRtcVideoChannel2Test, Vp8DenoisingEnabledByDefault) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoCodecVP8 vp8_settings;
  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  EXPECT_TRUE(vp8_settings.denoisingOn);
}

TEST_F(WebRtcVideoChannel2Test, VerifyVp8SpecificSettings) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(kVp8Codec720p);
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  // Single-stream settings should apply with RTX as well (verifies that we
  // check number of regular SSRCs and not StreamParams::ssrcs which contains
  // both RTX and regular SSRCs).
  FakeVideoSendStream* stream = SetUpSimulcast(false, true);

  cricket::FakeVideoCapturer capturer;
  capturer.SetScreencast(false);
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  EXPECT_TRUE(channel_->SetCapturer(last_ssrc_, &capturer));
  channel_->SetSend(true);

  EXPECT_TRUE(capturer.CaptureFrame());

  webrtc::VideoCodecVP8 vp8_settings;
  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  EXPECT_TRUE(vp8_settings.denoisingOn)
      << "VP8 denoising should be on by default.";

  stream = SetDenoisingOption(parameters, false);

  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  EXPECT_FALSE(vp8_settings.denoisingOn);
  EXPECT_TRUE(vp8_settings.automaticResizeOn);
  EXPECT_TRUE(vp8_settings.frameDroppingOn);

  stream = SetDenoisingOption(parameters, true);

  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  EXPECT_TRUE(vp8_settings.denoisingOn);
  EXPECT_TRUE(vp8_settings.automaticResizeOn);
  EXPECT_TRUE(vp8_settings.frameDroppingOn);

  EXPECT_TRUE(channel_->SetCapturer(last_ssrc_, NULL));
  stream = SetUpSimulcast(true, false);
  EXPECT_TRUE(channel_->SetCapturer(last_ssrc_, &capturer));
  channel_->SetSend(true);
  EXPECT_TRUE(capturer.CaptureFrame());

  EXPECT_EQ(3, stream->GetVideoStreams().size());
  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  // Autmatic resize off when using simulcast.
  EXPECT_FALSE(vp8_settings.automaticResizeOn);
  EXPECT_TRUE(vp8_settings.frameDroppingOn);

  // In screen-share mode, denoising is forced off and simulcast disabled.
  capturer.SetScreencast(true);
  EXPECT_TRUE(capturer.CaptureFrame());
  stream = SetDenoisingOption(parameters, false);

  EXPECT_EQ(1, stream->GetVideoStreams().size());
  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  EXPECT_FALSE(vp8_settings.denoisingOn);
  // Resizing and frame dropping always off for screen sharing.
  EXPECT_FALSE(vp8_settings.automaticResizeOn);
  EXPECT_FALSE(vp8_settings.frameDroppingOn);

  stream = SetDenoisingOption(parameters, true);

  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  EXPECT_FALSE(vp8_settings.denoisingOn);
  EXPECT_FALSE(vp8_settings.automaticResizeOn);
  EXPECT_FALSE(vp8_settings.frameDroppingOn);

  EXPECT_TRUE(channel_->SetCapturer(last_ssrc_, NULL));
}

class Vp9SettingsTest : public WebRtcVideoChannel2Test {
 public:
  Vp9SettingsTest() : WebRtcVideoChannel2Test() {
    encoder_factory_.AddSupportedVideoCodecType(webrtc::kVideoCodecVP9, "VP9");
  }
  virtual ~Vp9SettingsTest() {}

 protected:
  void SetUp() override {
    engine_.SetExternalEncoderFactory(&encoder_factory_);

    WebRtcVideoChannel2Test::SetUp();
  }

  void TearDown() override {
    // Remove references to encoder_factory_ since this will be destroyed
    // before channel_ and engine_.
    ASSERT_TRUE(channel_->SetSendParameters(send_parameters_));
  }

  cricket::FakeWebRtcVideoEncoderFactory encoder_factory_;
};

TEST_F(Vp9SettingsTest, VerifyVp9SpecificSettings) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(kVp9Codec);
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  FakeVideoSendStream* stream = SetUpSimulcast(false, false);

  cricket::FakeVideoCapturer capturer;
  capturer.SetScreencast(false);
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  EXPECT_TRUE(channel_->SetCapturer(last_ssrc_, &capturer));
  channel_->SetSend(true);

  EXPECT_TRUE(capturer.CaptureFrame());

  webrtc::VideoCodecVP9 vp9_settings;
  ASSERT_TRUE(stream->GetVp9Settings(&vp9_settings)) << "No VP9 config set.";
  EXPECT_FALSE(vp9_settings.denoisingOn)
      << "VP9 denoising should be off by default.";

  stream = SetDenoisingOption(parameters, false);

  ASSERT_TRUE(stream->GetVp9Settings(&vp9_settings)) << "No VP9 config set.";
  EXPECT_FALSE(vp9_settings.denoisingOn);
  // Frame dropping always on for real time video.
  EXPECT_TRUE(vp9_settings.frameDroppingOn);

  stream = SetDenoisingOption(parameters, true);

  ASSERT_TRUE(stream->GetVp9Settings(&vp9_settings)) << "No VP9 config set.";
  EXPECT_TRUE(vp9_settings.denoisingOn);
  EXPECT_TRUE(vp9_settings.frameDroppingOn);

  // In screen-share mode, denoising is forced off.
  capturer.SetScreencast(true);
  EXPECT_TRUE(capturer.CaptureFrame());
  stream = SetDenoisingOption(parameters, false);

  ASSERT_TRUE(stream->GetVp9Settings(&vp9_settings)) << "No VP9 config set.";
  EXPECT_FALSE(vp9_settings.denoisingOn);
  // Frame dropping always off for screen sharing.
  EXPECT_FALSE(vp9_settings.frameDroppingOn);

  stream = SetDenoisingOption(parameters, false);

  ASSERT_TRUE(stream->GetVp9Settings(&vp9_settings)) << "No VP9 config set.";
  EXPECT_FALSE(vp9_settings.denoisingOn);
  EXPECT_FALSE(vp9_settings.frameDroppingOn);

  EXPECT_TRUE(channel_->SetCapturer(last_ssrc_, NULL));
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_MultipleSendStreamsWithOneCapturer) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_SendReceiveBitratesStats) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, AdaptsOnOveruse) {
  TestCpuAdaptation(true, false);
}

TEST_F(WebRtcVideoChannel2Test, DoesNotAdaptOnOveruseWhenDisabled) {
  TestCpuAdaptation(false, false);
}

TEST_F(WebRtcVideoChannel2Test, DoesNotAdaptOnOveruseWhenScreensharing) {
  TestCpuAdaptation(true, true);
}

void WebRtcVideoChannel2Test::TestCpuAdaptation(bool enable_overuse,
                                                bool is_screenshare) {
  cricket::VideoCodec codec = kVp8Codec720p;
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(codec);
  if (!enable_overuse) {
    parameters.options.cpu_overuse_detection = rtc::Optional<bool>(false);
  }
  EXPECT_TRUE(channel_->SetSendParameters(parameters));

  AddSendStream();

  cricket::FakeVideoCapturer capturer;
  capturer.SetScreencast(is_screenshare);
  EXPECT_TRUE(channel_->SetCapturer(last_ssrc_, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));

  EXPECT_TRUE(channel_->SetSend(true));

  // Trigger overuse.
  ASSERT_EQ(1u, fake_call_->GetVideoSendStreams().size());
  FakeVideoSendStream* send_stream = fake_call_->GetVideoSendStreams().front();
  webrtc::LoadObserver* overuse_callback =
      send_stream->GetConfig().overuse_callback;
  ASSERT_TRUE(overuse_callback != NULL);
  overuse_callback->OnLoadUpdate(webrtc::LoadObserver::kOveruse);

  EXPECT_TRUE(capturer.CaptureFrame());
  EXPECT_EQ(1, send_stream->GetNumberOfSwappedFrames());

  if (enable_overuse && !is_screenshare) {
    EXPECT_LT(send_stream->GetLastWidth(), codec.width);
    EXPECT_LT(send_stream->GetLastHeight(), codec.height);
  } else {
    EXPECT_EQ(codec.width, send_stream->GetLastWidth());
    EXPECT_EQ(codec.height, send_stream->GetLastHeight());
  }

  // Trigger underuse which should go back to normal resolution.
  overuse_callback->OnLoadUpdate(webrtc::LoadObserver::kUnderuse);
  EXPECT_TRUE(capturer.CaptureFrame());

  EXPECT_EQ(2, send_stream->GetNumberOfSwappedFrames());

  EXPECT_EQ(codec.width, send_stream->GetLastWidth());
  EXPECT_EQ(codec.height, send_stream->GetLastHeight());

  EXPECT_TRUE(channel_->SetCapturer(last_ssrc_, NULL));
}

TEST_F(WebRtcVideoChannel2Test, EstimatesNtpStartTimeCorrectly) {
  // Start at last timestamp to verify that wraparounds are estimated correctly.
  static const uint32_t kInitialTimestamp = 0xFFFFFFFFu;
  static const int64_t kInitialNtpTimeMs = 1247891230;
  static const int kFrameOffsetMs = 20;
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));

  FakeVideoReceiveStream* stream = AddRecvStream();
  cricket::FakeVideoRenderer renderer;
  EXPECT_TRUE(channel_->SetRenderer(last_ssrc_, &renderer));

  webrtc::VideoFrame video_frame;
  CreateBlackFrame(&video_frame, 4, 4);
  video_frame.set_timestamp(kInitialTimestamp);
  // Initial NTP time is not available on the first frame, but should still be
  // able to be estimated.
  stream->InjectFrame(video_frame, 0);

  EXPECT_EQ(1, renderer.num_rendered_frames());

  // This timestamp is kInitialTimestamp (-1) + kFrameOffsetMs * 90, which
  // triggers a constant-overflow warning, hence we're calculating it explicitly
  // here.
  video_frame.set_timestamp(kFrameOffsetMs * 90 - 1);
  video_frame.set_ntp_time_ms(kInitialNtpTimeMs + kFrameOffsetMs);
  stream->InjectFrame(video_frame, 0);

  EXPECT_EQ(2, renderer.num_rendered_frames());

  // Verify that NTP time has been correctly deduced.
  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1u, info.receivers.size());
  EXPECT_EQ(kInitialNtpTimeMs, info.receivers[0].capture_start_ntp_time_ms);
}

TEST_F(WebRtcVideoChannel2Test, SetDefaultSendCodecs) {
  ASSERT_TRUE(channel_->SetSendParameters(send_parameters_));

  VideoCodec codec;
  EXPECT_TRUE(channel_->GetSendCodec(&codec));
  EXPECT_TRUE(codec.Matches(engine_.codecs()[0]));

  // Using a RTX setup to verify that the default RTX payload type is good.
  const std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs1);
  const std::vector<uint32_t> rtx_ssrcs = MAKE_VECTOR(kRtxSsrcs1);
  FakeVideoSendStream* stream = AddSendStream(
      cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs));
  webrtc::VideoSendStream::Config config = stream->GetConfig();

  // Make sure NACK and FEC are enabled on the correct payload types.
  EXPECT_EQ(1000, config.rtp.nack.rtp_history_ms);
  EXPECT_EQ(default_ulpfec_codec_.id, config.rtp.fec.ulpfec_payload_type);
  EXPECT_EQ(default_red_codec_.id, config.rtp.fec.red_payload_type);

  EXPECT_EQ(1u, config.rtp.rtx.ssrcs.size());
  EXPECT_EQ(kRtxSsrcs1[0], config.rtp.rtx.ssrcs[0]);
  VerifySendStreamHasRtxTypes(config, default_apt_rtx_types_);
  // TODO(juberti): Check RTCP, PLI, TMMBR.
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsWithoutFec) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(kVp8Codec);
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Config config = stream->GetConfig();

  EXPECT_EQ(-1, config.rtp.fec.ulpfec_payload_type);
  EXPECT_EQ(-1, config.rtp.fec.red_payload_type);
}

TEST_F(WebRtcVideoChannel2Test,
       SetSendCodecRejectsRtxWithoutAssociatedPayloadType) {
  cricket::VideoSendParameters parameters;
  cricket::VideoCodec rtx_codec(96, "rtx", 0, 0, 0, 0);
  parameters.codecs.push_back(rtx_codec);
  EXPECT_FALSE(channel_->SetSendParameters(parameters))
      << "RTX codec without associated payload type should be rejected.";
}

TEST_F(WebRtcVideoChannel2Test,
       SetSendCodecRejectsRtxWithoutMatchingVideoCodec) {
  cricket::VideoSendParameters parameters;
  cricket::VideoCodec rtx_codec =
      cricket::VideoCodec::CreateRtxCodec(96, kVp8Codec.id);
  parameters.codecs.push_back(kVp8Codec);
  parameters.codecs.push_back(rtx_codec);
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  cricket::VideoCodec rtx_codec2 =
      cricket::VideoCodec::CreateRtxCodec(96, kVp8Codec.id + 1);
  parameters.codecs.pop_back();
  parameters.codecs.push_back(rtx_codec2);
  EXPECT_FALSE(channel_->SetSendParameters(parameters))
      << "RTX without matching video codec should be rejected.";
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsWithoutFecDisablesFec) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(kVp8Codec);
  parameters.codecs.push_back(kUlpfecCodec);
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Config config = stream->GetConfig();

  EXPECT_EQ(kUlpfecCodec.id, config.rtp.fec.ulpfec_payload_type);

  parameters.codecs.pop_back();
  ASSERT_TRUE(channel_->SetSendParameters(parameters));
  stream = fake_call_->GetVideoSendStreams()[0];
  ASSERT_TRUE(stream != NULL);
  config = stream->GetConfig();
  EXPECT_EQ(-1, config.rtp.fec.ulpfec_payload_type)
      << "SetSendCodec without FEC should disable current FEC.";
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsChangesExistingStreams) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(kVp8Codec720p);
  ASSERT_TRUE(channel_->SetSendParameters(parameters));
  channel_->SetSend(true);

  FakeVideoSendStream* stream = AddSendStream();

  cricket::FakeVideoCapturer capturer;
  EXPECT_TRUE(channel_->SetCapturer(last_ssrc_, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  EXPECT_TRUE(capturer.CaptureFrame());

  std::vector<webrtc::VideoStream> streams = stream->GetVideoStreams();
  EXPECT_EQ(kVp8Codec720p.width, streams[0].width);
  EXPECT_EQ(kVp8Codec720p.height, streams[0].height);

  parameters.codecs.clear();
  parameters.codecs.push_back(kVp8Codec360p);
  ASSERT_TRUE(channel_->SetSendParameters(parameters));
  streams = fake_call_->GetVideoSendStreams()[0]->GetVideoStreams();
  EXPECT_EQ(kVp8Codec360p.width, streams[0].width);
  EXPECT_EQ(kVp8Codec360p.height, streams[0].height);
  EXPECT_TRUE(channel_->SetCapturer(last_ssrc_, NULL));
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsWithBitrates) {
  SetSendCodecsShouldWorkForBitrates("100", 100000, "150", 150000, "200",
                                     200000);
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsWithHighMaxBitrate) {
  SetSendCodecsShouldWorkForBitrates("", 0, "", -1, "10000", 10000000);
  std::vector<webrtc::VideoStream> streams = AddSendStream()->GetVideoStreams();
  ASSERT_EQ(1u, streams.size());
  EXPECT_EQ(10000000, streams[0].max_bitrate_bps);
}

TEST_F(WebRtcVideoChannel2Test,
       SetSendCodecsWithoutBitratesUsesCorrectDefaults) {
  SetSendCodecsShouldWorkForBitrates(
      "", 0, "", -1, "", -1);
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsCapsMinAndStartBitrate) {
  SetSendCodecsShouldWorkForBitrates("-1", 0, "-100", -1, "", -1);
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsRejectsMaxLessThanMinBitrate) {
  send_parameters_.codecs[0].params[kCodecParamMinBitrate] = "300";
  send_parameters_.codecs[0].params[kCodecParamMaxBitrate] = "200";
  EXPECT_FALSE(channel_->SetSendParameters(send_parameters_));
}

TEST_F(WebRtcVideoChannel2Test,
       SetMaxSendBandwidthShouldPreserveOtherBitrates) {
  SetSendCodecsShouldWorkForBitrates("100", 100000, "150", 150000, "200",
                                     200000);
  send_parameters_.max_bandwidth_bps = 300000;
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  EXPECT_EQ(100000, fake_call_->GetConfig().bitrate_config.min_bitrate_bps)
      << "Setting max bitrate should keep previous min bitrate.";
  EXPECT_EQ(-1, fake_call_->GetConfig().bitrate_config.start_bitrate_bps)
      << "Setting max bitrate should not reset start bitrate.";
  EXPECT_EQ(300000, fake_call_->GetConfig().bitrate_config.max_bitrate_bps);
}

TEST_F(WebRtcVideoChannel2Test, SetMaxSendBandwidthShouldBeRemovable) {
  send_parameters_.max_bandwidth_bps = 300000;
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  EXPECT_EQ(300000, fake_call_->GetConfig().bitrate_config.max_bitrate_bps);
  // <= 0 means disable (infinite) max bitrate.
  send_parameters_.max_bandwidth_bps = 0;
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  EXPECT_EQ(-1, fake_call_->GetConfig().bitrate_config.max_bitrate_bps)
      << "Setting zero max bitrate did not reset start bitrate.";
}

TEST_F(WebRtcVideoChannel2Test, SetMaxSendBitrateCanIncreaseSenderBitrate) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(kVp8Codec720p);
  ASSERT_TRUE(channel_->SetSendParameters(parameters));
  channel_->SetSend(true);

  FakeVideoSendStream* stream = AddSendStream();

  std::vector<webrtc::VideoStream> streams = stream->GetVideoStreams();
  int initial_max_bitrate_bps = streams[0].max_bitrate_bps;
  EXPECT_GT(initial_max_bitrate_bps, 0);

  parameters.max_bandwidth_bps = initial_max_bitrate_bps * 2;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  streams = stream->GetVideoStreams();
  EXPECT_EQ(initial_max_bitrate_bps * 2, streams[0].max_bitrate_bps);
}

TEST_F(WebRtcVideoChannel2Test,
       SetMaxSendBitrateCanIncreaseSimulcastSenderBitrate) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(kVp8Codec720p);
  ASSERT_TRUE(channel_->SetSendParameters(parameters));
  channel_->SetSend(true);

  FakeVideoSendStream* stream = AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs3)));

  // Send a frame to make sure this scales up to >1 stream (simulcast).
  cricket::FakeVideoCapturer capturer;
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs3[0], &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  EXPECT_TRUE(capturer.CaptureFrame());

  std::vector<webrtc::VideoStream> streams = stream->GetVideoStreams();
  ASSERT_GT(streams.size(), 1u)
      << "Without simulcast this test doesn't make sense.";
  int initial_max_bitrate_bps = GetTotalMaxBitrateBps(streams);
  EXPECT_GT(initial_max_bitrate_bps, 0);

  parameters.max_bandwidth_bps = initial_max_bitrate_bps * 2;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  streams = stream->GetVideoStreams();
  int increased_max_bitrate_bps = GetTotalMaxBitrateBps(streams);
  EXPECT_EQ(initial_max_bitrate_bps * 2, increased_max_bitrate_bps);

  EXPECT_TRUE(channel_->SetCapturer(kSsrcs3[0], nullptr));
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsWithMaxQuantization) {
  static const char* kMaxQuantization = "21";
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(kVp8Codec);
  parameters.codecs[0].params[kCodecParamMaxQuantization] = kMaxQuantization;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(static_cast<unsigned int>(atoi(kMaxQuantization)),
            AddSendStream()->GetVideoStreams().back().max_qp);

  VideoCodec codec;
  EXPECT_TRUE(channel_->GetSendCodec(&codec));
  EXPECT_EQ(kMaxQuantization, codec.params[kCodecParamMaxQuantization]);
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsRejectBadDimensions) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(kVp8Codec);

  parameters.codecs[0].width = 0;
  EXPECT_FALSE(channel_->SetSendParameters(parameters))
      << "Codec set though codec width is zero.";

  parameters.codecs[0].width = kVp8Codec.width;
  parameters.codecs[0].height = 0;
  EXPECT_FALSE(channel_->SetSendParameters(parameters))
      << "Codec set though codec height is zero.";
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsRejectBadPayloadTypes) {
  // TODO(pbos): Should we only allow the dynamic range?
  static const int kIncorrectPayloads[] = {-2, -1, 128, 129};
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(kVp8Codec);
  for (size_t i = 0; i < arraysize(kIncorrectPayloads); ++i) {
    parameters.codecs[0].id = kIncorrectPayloads[i];
    EXPECT_FALSE(channel_->SetSendParameters(parameters))
        << "Bad payload type '" << kIncorrectPayloads[i] << "' accepted.";
  }
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsAcceptAllValidPayloadTypes) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(kVp8Codec);
  for (int payload_type = 0; payload_type <= 127; ++payload_type) {
    parameters.codecs[0].id = payload_type;
    EXPECT_TRUE(channel_->SetSendParameters(parameters))
        << "Payload type '" << payload_type << "' rejected.";
  }
}

TEST_F(WebRtcVideoChannel2Test, SetRecvCodecsWithOnlyVp8) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(kVp8Codec);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
}

// Test that we set our inbound RTX codecs properly.
TEST_F(WebRtcVideoChannel2Test, SetRecvCodecsWithRtx) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(kVp8Codec);
  cricket::VideoCodec rtx_codec(96, "rtx", 0, 0, 0, 0);
  parameters.codecs.push_back(rtx_codec);
  EXPECT_FALSE(channel_->SetRecvParameters(parameters))
      << "RTX codec without associated payload should be rejected.";

  parameters.codecs[1].SetParam("apt", kVp8Codec.id + 1);
  EXPECT_FALSE(channel_->SetRecvParameters(parameters))
      << "RTX codec with invalid associated payload type should be rejected.";

  parameters.codecs[1].SetParam("apt", kVp8Codec.id);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  cricket::VideoCodec rtx_codec2(97, "rtx", 0, 0, 0, 0);
  rtx_codec2.SetParam("apt", rtx_codec.id);
  parameters.codecs.push_back(rtx_codec2);

  EXPECT_FALSE(channel_->SetRecvParameters(parameters)) <<
      "RTX codec with another RTX as associated payload type should be "
      "rejected.";
}

TEST_F(WebRtcVideoChannel2Test, SetRecvCodecsDifferentPayloadType) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(kVp8Codec);
  parameters.codecs[0].id = 99;
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
}

TEST_F(WebRtcVideoChannel2Test, SetRecvCodecsAcceptDefaultCodecs) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs = engine_.codecs();
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  FakeVideoReceiveStream* stream = AddRecvStream();
  webrtc::VideoReceiveStream::Config config = stream->GetConfig();
  EXPECT_EQ(engine_.codecs()[0].name, config.decoders[0].payload_name);
  EXPECT_EQ(engine_.codecs()[0].id, config.decoders[0].payload_type);
}

TEST_F(WebRtcVideoChannel2Test, SetRecvCodecsRejectUnsupportedCodec) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(kVp8Codec);
  parameters.codecs.push_back(VideoCodec(101, "WTF3", 640, 400, 30, 0));
  EXPECT_FALSE(channel_->SetRecvParameters(parameters));
}

// TODO(pbos): Enable VP9 through external codec support
TEST_F(WebRtcVideoChannel2Test,
       DISABLED_SetRecvCodecsAcceptsMultipleVideoCodecs) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(kVp8Codec);
  parameters.codecs.push_back(kVp9Codec);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
}

TEST_F(WebRtcVideoChannel2Test,
       DISABLED_SetRecvCodecsSetsFecForAllVideoCodecs) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(kVp8Codec);
  parameters.codecs.push_back(kVp9Codec);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  FAIL();  // TODO(pbos): Verify that the FEC parameters are set for all codecs.
}

TEST_F(WebRtcVideoChannel2Test, SetRecvCodecsWithoutFecDisablesFec) {
  cricket::VideoSendParameters send_parameters;
  send_parameters.codecs.push_back(kVp8Codec);
  send_parameters.codecs.push_back(kUlpfecCodec);
  ASSERT_TRUE(channel_->SetSendParameters(send_parameters));

  FakeVideoReceiveStream* stream = AddRecvStream();
  webrtc::VideoReceiveStream::Config config = stream->GetConfig();

  EXPECT_EQ(kUlpfecCodec.id, config.rtp.fec.ulpfec_payload_type);

  cricket::VideoRecvParameters recv_parameters;
  recv_parameters.codecs.push_back(kVp8Codec);
  ASSERT_TRUE(channel_->SetRecvParameters(recv_parameters));
  stream = fake_call_->GetVideoReceiveStreams()[0];
  ASSERT_TRUE(stream != NULL);
  config = stream->GetConfig();
  EXPECT_EQ(-1, config.rtp.fec.ulpfec_payload_type)
      << "SetSendCodec without FEC should disable current FEC.";
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsRejectDuplicateFecPayloads) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(kVp8Codec);
  parameters.codecs.push_back(kRedCodec);
  parameters.codecs[1].id = parameters.codecs[0].id;
  EXPECT_FALSE(channel_->SetRecvParameters(parameters));
}

TEST_F(WebRtcVideoChannel2Test, SetRecvCodecsRejectDuplicateCodecPayloads) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(kVp8Codec);
  parameters.codecs.push_back(kVp9Codec);
  parameters.codecs[1].id = parameters.codecs[0].id;
  EXPECT_FALSE(channel_->SetRecvParameters(parameters));
}

TEST_F(WebRtcVideoChannel2Test,
       SetRecvCodecsAcceptSameCodecOnMultiplePayloadTypes) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(kVp8Codec);
  parameters.codecs.push_back(kVp8Codec);
  parameters.codecs[1].id += 1;
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
}

// Test that setting the same codecs but with a different order and preference
// doesn't result in the stream being recreated.
TEST_F(WebRtcVideoChannel2Test,
       SetRecvCodecsDifferentOrderAndPreferenceDoesntRecreateStream) {
  cricket::VideoRecvParameters parameters1;
  parameters1.codecs.push_back(kVp8Codec);
  parameters1.codecs.push_back(kRedCodec);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters1));

  AddRecvStream(cricket::StreamParams::CreateLegacy(123));
  EXPECT_EQ(1, fake_call_->GetNumCreatedReceiveStreams());

  cricket::VideoRecvParameters parameters2;
  parameters2.codecs.push_back(kRedCodec);
  parameters2.codecs.push_back(kVp8Codec);
  parameters2.codecs[1].preference += 1;
  EXPECT_TRUE(channel_->SetRecvParameters(parameters2));
  EXPECT_EQ(1, fake_call_->GetNumCreatedReceiveStreams());
}

TEST_F(WebRtcVideoChannel2Test, SendStreamNotSendingByDefault) {
  EXPECT_FALSE(AddSendStream()->IsSending());
}

TEST_F(WebRtcVideoChannel2Test, ReceiveStreamReceivingByDefault) {
  EXPECT_TRUE(AddRecvStream()->IsReceiving());
}

TEST_F(WebRtcVideoChannel2Test, SetSend) {
  FakeVideoSendStream* stream = AddSendStream();
  EXPECT_FALSE(stream->IsSending());

  // false->true
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_TRUE(stream->IsSending());
  // true->true
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_TRUE(stream->IsSending());
  // true->false
  EXPECT_TRUE(channel_->SetSend(false));
  EXPECT_FALSE(stream->IsSending());
  // false->false
  EXPECT_TRUE(channel_->SetSend(false));
  EXPECT_FALSE(stream->IsSending());

  EXPECT_TRUE(channel_->SetSend(true));
  FakeVideoSendStream* new_stream = AddSendStream();
  EXPECT_TRUE(new_stream->IsSending())
      << "Send stream created after SetSend(true) not sending initially.";
}

// This test verifies DSCP settings are properly applied on video media channel.
TEST_F(WebRtcVideoChannel2Test, TestSetDscpOptions) {
  rtc::scoped_ptr<cricket::FakeNetworkInterface> network_interface(
      new cricket::FakeNetworkInterface);
  channel_->SetInterface(network_interface.get());
  cricket::VideoSendParameters parameters = send_parameters_;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(rtc::DSCP_NO_CHANGE, network_interface->dscp());
  parameters.options.dscp = rtc::Optional<bool>(true);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(rtc::DSCP_AF41, network_interface->dscp());
  // Verify previous value is not modified if dscp option is not set.
  cricket::VideoSendParameters parameters1 = send_parameters_;
  EXPECT_TRUE(channel_->SetSendParameters(parameters1));
  EXPECT_EQ(rtc::DSCP_AF41, network_interface->dscp());
  parameters1.options.dscp = rtc::Optional<bool>(false);
  EXPECT_TRUE(channel_->SetSendParameters(parameters1));
  EXPECT_EQ(rtc::DSCP_DEFAULT, network_interface->dscp());
  channel_->SetInterface(NULL);
}

// This test verifies that the RTCP reduced size mode is properly applied to
// send video streams.
TEST_F(WebRtcVideoChannel2Test, TestSetSendRtcpReducedSize) {
  // Create stream, expecting that default mode is "compound".
  FakeVideoSendStream* stream1 = AddSendStream();
  EXPECT_EQ(webrtc::RtcpMode::kCompound, stream1->GetConfig().rtp.rtcp_mode);

  // Now enable reduced size mode.
  send_parameters_.rtcp.reduced_size = true;
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  stream1 = fake_call_->GetVideoSendStreams()[0];
  EXPECT_EQ(webrtc::RtcpMode::kReducedSize, stream1->GetConfig().rtp.rtcp_mode);

  // Create a new stream and ensure it picks up the reduced size mode.
  FakeVideoSendStream* stream2 = AddSendStream();
  EXPECT_EQ(webrtc::RtcpMode::kReducedSize, stream2->GetConfig().rtp.rtcp_mode);
}

// This test verifies that the RTCP reduced size mode is properly applied to
// receive video streams.
TEST_F(WebRtcVideoChannel2Test, TestSetRecvRtcpReducedSize) {
  // Create stream, expecting that default mode is "compound".
  FakeVideoReceiveStream* stream1 = AddRecvStream();
  EXPECT_EQ(webrtc::RtcpMode::kCompound, stream1->GetConfig().rtp.rtcp_mode);

  // Now enable reduced size mode.
  recv_parameters_.rtcp.reduced_size = true;
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));
  stream1 = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_EQ(webrtc::RtcpMode::kReducedSize, stream1->GetConfig().rtp.rtcp_mode);

  // Create a new stream and ensure it picks up the reduced size mode.
  FakeVideoReceiveStream* stream2 = AddRecvStream();
  EXPECT_EQ(webrtc::RtcpMode::kReducedSize, stream2->GetConfig().rtp.rtcp_mode);
}

TEST_F(WebRtcVideoChannel2Test, OnReadyToSendSignalsNetworkState) {
  EXPECT_EQ(webrtc::kNetworkUp, fake_call_->GetNetworkState());

  channel_->OnReadyToSend(false);
  EXPECT_EQ(webrtc::kNetworkDown, fake_call_->GetNetworkState());

  channel_->OnReadyToSend(true);
  EXPECT_EQ(webrtc::kNetworkUp, fake_call_->GetNetworkState());
}

TEST_F(WebRtcVideoChannel2Test, GetStatsReportsSentCodecName) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(kVp8Codec);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));

  AddSendStream();

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ(kVp8Codec.name, info.senders[0].codec_name);
}

TEST_F(WebRtcVideoChannel2Test, GetStatsReportsEncoderImplementationName) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.encoder_implementation_name = "encoder_implementation_name";
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ(stats.encoder_implementation_name,
            info.senders[0].encoder_implementation_name);
}

TEST_F(WebRtcVideoChannel2Test, GetStatsReportsCpuOveruseMetrics) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.avg_encode_time_ms = 13;
  stats.encode_usage_percent = 42;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ(stats.avg_encode_time_ms, info.senders[0].avg_encode_ms);
  EXPECT_EQ(stats.encode_usage_percent, info.senders[0].encode_usage_percent);
}

TEST_F(WebRtcVideoChannel2Test, GetStatsReportsUpperResolution) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.substreams[17].width = 123;
  stats.substreams[17].height = 40;
  stats.substreams[42].width = 80;
  stats.substreams[42].height = 31;
  stats.substreams[11].width = 20;
  stats.substreams[11].height = 90;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1u, info.senders.size());
  EXPECT_EQ(123, info.senders[0].send_frame_width);
  EXPECT_EQ(90, info.senders[0].send_frame_height);
}

TEST_F(WebRtcVideoChannel2Test, GetStatsTracksAdaptationStats) {
  AddSendStream(cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs3)));

  // Capture format VGA.
  cricket::FakeVideoCapturer video_capturer_vga;
  const std::vector<cricket::VideoFormat>* formats =
      video_capturer_vga.GetSupportedFormats();
  cricket::VideoFormat capture_format_vga = (*formats)[1];
  EXPECT_EQ(cricket::CS_RUNNING, video_capturer_vga.Start(capture_format_vga));
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs3[0], &video_capturer_vga));
  EXPECT_TRUE(video_capturer_vga.CaptureFrame());

  cricket::VideoCodec send_codec(100, "VP8", 640, 480, 30, 0);
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(send_codec);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_TRUE(channel_->SetSend(true));

  // Verify that the CpuOveruseObserver is registered and trigger downgrade.
  parameters.options.cpu_overuse_detection = rtc::Optional<bool>(true);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));

  // Trigger overuse.
  ASSERT_EQ(1u, fake_call_->GetVideoSendStreams().size());
  webrtc::LoadObserver* overuse_callback =
      fake_call_->GetVideoSendStreams().front()->GetConfig().overuse_callback;
  ASSERT_TRUE(overuse_callback != NULL);
  overuse_callback->OnLoadUpdate(webrtc::LoadObserver::kOveruse);

  // Capture format VGA -> adapt (OnCpuResolutionRequest downgrade) -> VGA/2.
  EXPECT_TRUE(video_capturer_vga.CaptureFrame());
  cricket::VideoMediaInfo info;
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.senders.size());
  EXPECT_EQ(1, info.senders[0].adapt_changes);
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_CPU,
            info.senders[0].adapt_reason);

  // Trigger upgrade and verify that we adapt back up to VGA.
  overuse_callback->OnLoadUpdate(webrtc::LoadObserver::kUnderuse);
  EXPECT_TRUE(video_capturer_vga.CaptureFrame());
  info.Clear();
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.senders.size());
  EXPECT_EQ(2, info.senders[0].adapt_changes);
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_NONE,
            info.senders[0].adapt_reason);

  // No capturer (no adapter). Adapt changes from old adapter should be kept.
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs3[0], NULL));
  info.Clear();
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.senders.size());
  EXPECT_EQ(2, info.senders[0].adapt_changes);
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_NONE,
            info.senders[0].adapt_reason);

  // Set new capturer, capture format HD.
  cricket::FakeVideoCapturer video_capturer_hd;
  cricket::VideoFormat capture_format_hd = (*formats)[0];
  EXPECT_EQ(cricket::CS_RUNNING, video_capturer_hd.Start(capture_format_hd));
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs3[0], &video_capturer_hd));
  EXPECT_TRUE(video_capturer_hd.CaptureFrame());

  // Trigger overuse, HD -> adapt (OnCpuResolutionRequest downgrade) -> HD/2.
  overuse_callback->OnLoadUpdate(webrtc::LoadObserver::kOveruse);
  EXPECT_TRUE(video_capturer_hd.CaptureFrame());
  info.Clear();
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.senders.size());
  EXPECT_EQ(3, info.senders[0].adapt_changes);
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_CPU,
            info.senders[0].adapt_reason);

  EXPECT_TRUE(channel_->SetCapturer(kSsrcs3[0], NULL));
}

TEST_F(WebRtcVideoChannel2Test, GetStatsTracksAdaptationAndBandwidthStats) {
  AddSendStream(cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs3)));

  // Capture format VGA.
  cricket::FakeVideoCapturer video_capturer_vga;
  const std::vector<cricket::VideoFormat>* formats =
      video_capturer_vga.GetSupportedFormats();
  cricket::VideoFormat capture_format_vga = (*formats)[1];
  EXPECT_EQ(cricket::CS_RUNNING, video_capturer_vga.Start(capture_format_vga));
  EXPECT_TRUE(channel_->SetCapturer(kSsrcs3[0], &video_capturer_vga));
  EXPECT_TRUE(video_capturer_vga.CaptureFrame());

  cricket::VideoCodec send_codec(100, "VP8", 640, 480, 30, 0);
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(send_codec);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_TRUE(channel_->SetSend(true));

  // Verify that the CpuOveruseObserver is registered and trigger downgrade.
  parameters.options.cpu_overuse_detection = rtc::Optional<bool>(true);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));

  // Trigger overuse -> adapt CPU.
  ASSERT_EQ(1u, fake_call_->GetVideoSendStreams().size());
  webrtc::LoadObserver* overuse_callback =
      fake_call_->GetVideoSendStreams().front()->GetConfig().overuse_callback;
  ASSERT_TRUE(overuse_callback != NULL);
  overuse_callback->OnLoadUpdate(webrtc::LoadObserver::kOveruse);
  EXPECT_TRUE(video_capturer_vga.CaptureFrame());
  cricket::VideoMediaInfo info;
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.senders.size());
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_CPU,
            info.senders[0].adapt_reason);

  // Set bandwidth limitation stats for the stream -> adapt CPU + BW.
  webrtc::VideoSendStream::Stats stats;
  stats.bw_limited_resolution = true;
  fake_call_->GetVideoSendStreams().front()->SetStats(stats);
  info.Clear();
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.senders.size());
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_CPU +
            CoordinatedVideoAdapter::ADAPTREASON_BANDWIDTH,
            info.senders[0].adapt_reason);

  // Trigger upgrade -> adapt BW.
  overuse_callback->OnLoadUpdate(webrtc::LoadObserver::kUnderuse);
  EXPECT_TRUE(video_capturer_vga.CaptureFrame());
  info.Clear();
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.senders.size());
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_BANDWIDTH,
            info.senders[0].adapt_reason);

  // Reset bandwidth limitation state -> adapt NONE.
  stats.bw_limited_resolution = false;
  fake_call_->GetVideoSendStreams().front()->SetStats(stats);
  info.Clear();
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.senders.size());
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_NONE,
            info.senders[0].adapt_reason);

  EXPECT_TRUE(channel_->SetCapturer(kSsrcs3[0], NULL));
}

TEST_F(WebRtcVideoChannel2Test,
       GetStatsTranslatesBandwidthLimitedResolutionCorrectly) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.bw_limited_resolution = true;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.senders.size());
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_BANDWIDTH,
            info.senders[0].adapt_reason);
}

TEST_F(WebRtcVideoChannel2Test,
       GetStatsTranslatesSendRtcpPacketTypesCorrectly) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.substreams[17].rtcp_packet_type_counts.fir_packets = 2;
  stats.substreams[17].rtcp_packet_type_counts.nack_packets = 3;
  stats.substreams[17].rtcp_packet_type_counts.pli_packets = 4;

  stats.substreams[42].rtcp_packet_type_counts.fir_packets = 5;
  stats.substreams[42].rtcp_packet_type_counts.nack_packets = 7;
  stats.substreams[42].rtcp_packet_type_counts.pli_packets = 9;

  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ(7, info.senders[0].firs_rcvd);
  EXPECT_EQ(10, info.senders[0].nacks_rcvd);
  EXPECT_EQ(13, info.senders[0].plis_rcvd);
}

TEST_F(WebRtcVideoChannel2Test,
       GetStatsTranslatesReceiveRtcpPacketTypesCorrectly) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  webrtc::VideoReceiveStream::Stats stats;
  stats.rtcp_packet_type_counts.fir_packets = 2;
  stats.rtcp_packet_type_counts.nack_packets = 3;
  stats.rtcp_packet_type_counts.pli_packets = 4;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ(stats.rtcp_packet_type_counts.fir_packets,
            info.receivers[0].firs_sent);
  EXPECT_EQ(stats.rtcp_packet_type_counts.nack_packets,
            info.receivers[0].nacks_sent);
  EXPECT_EQ(stats.rtcp_packet_type_counts.pli_packets,
            info.receivers[0].plis_sent);
}

TEST_F(WebRtcVideoChannel2Test, GetStatsTranslatesDecodeStatsCorrectly) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  webrtc::VideoReceiveStream::Stats stats;
  stats.decoder_implementation_name = "decoder_implementation_name";
  stats.decode_ms = 2;
  stats.max_decode_ms = 3;
  stats.current_delay_ms = 4;
  stats.target_delay_ms = 5;
  stats.jitter_buffer_ms = 6;
  stats.min_playout_delay_ms = 7;
  stats.render_delay_ms = 8;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ(stats.decoder_implementation_name,
            info.receivers[0].decoder_implementation_name);
  EXPECT_EQ(stats.decode_ms, info.receivers[0].decode_ms);
  EXPECT_EQ(stats.max_decode_ms, info.receivers[0].max_decode_ms);
  EXPECT_EQ(stats.current_delay_ms, info.receivers[0].current_delay_ms);
  EXPECT_EQ(stats.target_delay_ms, info.receivers[0].target_delay_ms);
  EXPECT_EQ(stats.jitter_buffer_ms, info.receivers[0].jitter_buffer_ms);
  EXPECT_EQ(stats.min_playout_delay_ms, info.receivers[0].min_playout_delay_ms);
  EXPECT_EQ(stats.render_delay_ms, info.receivers[0].render_delay_ms);
}

TEST_F(WebRtcVideoChannel2Test, GetStatsTranslatesReceivePacketStatsCorrectly) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  webrtc::VideoReceiveStream::Stats stats;
  stats.rtp_stats.transmitted.payload_bytes = 2;
  stats.rtp_stats.transmitted.header_bytes = 3;
  stats.rtp_stats.transmitted.padding_bytes = 4;
  stats.rtp_stats.transmitted.packets = 5;
  stats.rtcp_stats.cumulative_lost = 6;
  stats.rtcp_stats.fraction_lost = 7;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ(stats.rtp_stats.transmitted.payload_bytes +
                stats.rtp_stats.transmitted.header_bytes +
                stats.rtp_stats.transmitted.padding_bytes,
            info.receivers[0].bytes_rcvd);
  EXPECT_EQ(stats.rtp_stats.transmitted.packets,
            info.receivers[0].packets_rcvd);
  EXPECT_EQ(stats.rtcp_stats.cumulative_lost, info.receivers[0].packets_lost);
  EXPECT_EQ(static_cast<float>(stats.rtcp_stats.fraction_lost) / (1 << 8),
            info.receivers[0].fraction_lost);
}

TEST_F(WebRtcVideoChannel2Test, TranslatesCallStatsCorrectly) {
  AddSendStream();
  AddSendStream();
  webrtc::Call::Stats stats;
  stats.rtt_ms = 123;
  fake_call_->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(2u, info.senders.size());
  EXPECT_EQ(stats.rtt_ms, info.senders[0].rtt_ms);
  EXPECT_EQ(stats.rtt_ms, info.senders[1].rtt_ms);
}

TEST_F(WebRtcVideoChannel2Test, TranslatesSenderBitrateStatsCorrectly) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.target_media_bitrate_bps = 156;
  stats.media_bitrate_bps = 123;
  stats.substreams[17].total_bitrate_bps = 1;
  stats.substreams[17].retransmit_bitrate_bps = 2;
  stats.substreams[42].total_bitrate_bps = 3;
  stats.substreams[42].retransmit_bitrate_bps = 4;
  stream->SetStats(stats);

  FakeVideoSendStream* stream2 = AddSendStream();
  webrtc::VideoSendStream::Stats stats2;
  stats2.target_media_bitrate_bps = 200;
  stats2.media_bitrate_bps = 321;
  stats2.substreams[13].total_bitrate_bps = 5;
  stats2.substreams[13].retransmit_bitrate_bps = 6;
  stats2.substreams[21].total_bitrate_bps = 7;
  stats2.substreams[21].retransmit_bitrate_bps = 8;
  stream2->SetStats(stats2);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(2u, info.senders.size());
  // Assuming stream and stream2 corresponds to senders[0] and [1] respectively
  // is OK as std::maps are sorted and AddSendStream() gives increasing SSRCs.
  EXPECT_EQ(stats.media_bitrate_bps, info.senders[0].nominal_bitrate);
  EXPECT_EQ(stats2.media_bitrate_bps, info.senders[1].nominal_bitrate);
  EXPECT_EQ(stats.target_media_bitrate_bps + stats2.target_media_bitrate_bps,
            info.bw_estimations[0].target_enc_bitrate);
  EXPECT_EQ(stats.media_bitrate_bps + stats2.media_bitrate_bps,
            info.bw_estimations[0].actual_enc_bitrate);
  EXPECT_EQ(1 + 3 + 5 + 7, info.bw_estimations[0].transmit_bitrate)
      << "Bandwidth stats should take all streams into account.";
  EXPECT_EQ(2 + 4 + 6 + 8, info.bw_estimations[0].retransmit_bitrate)
      << "Bandwidth stats should take all streams into account.";
}

TEST_F(WebRtcVideoChannel2Test, DefaultReceiveStreamReconfiguresToUseRtx) {
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  const std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs1);
  const std::vector<uint32_t> rtx_ssrcs = MAKE_VECTOR(kRtxSsrcs1);

  ASSERT_EQ(0u, fake_call_->GetVideoReceiveStreams().size());
  const size_t kDataLength = 12;
  uint8_t data[kDataLength];
  memset(data, 0, sizeof(data));
  rtc::SetBE32(&data[8], ssrcs[0]);
  rtc::Buffer packet(data, kDataLength);
  rtc::PacketTime packet_time;
  channel_->OnPacketReceived(&packet, packet_time);

  ASSERT_EQ(1u, fake_call_->GetVideoReceiveStreams().size())
      << "No default receive stream created.";
  FakeVideoReceiveStream* recv_stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_EQ(0u, recv_stream->GetConfig().rtp.rtx.size())
      << "Default receive stream should not have configured RTX";

  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs)));
  ASSERT_EQ(1u, fake_call_->GetVideoReceiveStreams().size())
      << "AddRecvStream should've reconfigured, not added a new receiver.";
  recv_stream = fake_call_->GetVideoReceiveStreams()[0];
  ASSERT_EQ(1u, recv_stream->GetConfig().rtp.rtx.size());
  EXPECT_EQ(rtx_ssrcs[0],
            recv_stream->GetConfig().rtp.rtx.begin()->second.ssrc);
}

TEST_F(WebRtcVideoChannel2Test, RejectsAddingStreamsWithMissingSsrcsForRtx) {
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  const std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs1);
  const std::vector<uint32_t> rtx_ssrcs = MAKE_VECTOR(kRtxSsrcs1);

  StreamParams sp =
      cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs);
  sp.ssrcs = ssrcs;  // Without RTXs, this is the important part.

  EXPECT_FALSE(channel_->AddSendStream(sp));
  EXPECT_FALSE(channel_->AddRecvStream(sp));
}

TEST_F(WebRtcVideoChannel2Test, RejectsAddingStreamsWithOverlappingRtxSsrcs) {
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  const std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs1);
  const std::vector<uint32_t> rtx_ssrcs = MAKE_VECTOR(kRtxSsrcs1);

  StreamParams sp =
      cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs);

  EXPECT_TRUE(channel_->AddSendStream(sp));
  EXPECT_TRUE(channel_->AddRecvStream(sp));

  // The RTX SSRC is already used in previous streams, using it should fail.
  sp = cricket::StreamParams::CreateLegacy(rtx_ssrcs[0]);
  EXPECT_FALSE(channel_->AddSendStream(sp));
  EXPECT_FALSE(channel_->AddRecvStream(sp));

  // After removing the original stream this should be fine to add (makes sure
  // that RTX ssrcs are not forever taken).
  EXPECT_TRUE(channel_->RemoveSendStream(ssrcs[0]));
  EXPECT_TRUE(channel_->RemoveRecvStream(ssrcs[0]));
  EXPECT_TRUE(channel_->AddSendStream(sp));
  EXPECT_TRUE(channel_->AddRecvStream(sp));
}

TEST_F(WebRtcVideoChannel2Test,
       RejectsAddingStreamsWithOverlappingSimulcastSsrcs) {
  static const uint32_t kFirstStreamSsrcs[] = {1, 2, 3};
  static const uint32_t kOverlappingStreamSsrcs[] = {4, 3, 5};
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  StreamParams sp =
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kFirstStreamSsrcs));

  EXPECT_TRUE(channel_->AddSendStream(sp));
  EXPECT_TRUE(channel_->AddRecvStream(sp));

  // One of the SSRCs is already used in previous streams, using it should fail.
  sp = cricket::CreateSimStreamParams("cname",
                                      MAKE_VECTOR(kOverlappingStreamSsrcs));
  EXPECT_FALSE(channel_->AddSendStream(sp));
  EXPECT_FALSE(channel_->AddRecvStream(sp));

  // After removing the original stream this should be fine to add (makes sure
  // that RTX ssrcs are not forever taken).
  EXPECT_TRUE(channel_->RemoveSendStream(kFirstStreamSsrcs[0]));
  EXPECT_TRUE(channel_->RemoveRecvStream(kFirstStreamSsrcs[0]));
  EXPECT_TRUE(channel_->AddSendStream(sp));
  EXPECT_TRUE(channel_->AddRecvStream(sp));
}

TEST_F(WebRtcVideoChannel2Test, ReportsSsrcGroupsInStats) {
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  static const uint32_t kSenderSsrcs[] = {4, 7, 10};
  static const uint32_t kSenderRtxSsrcs[] = {5, 8, 11};

  StreamParams sender_sp = cricket::CreateSimWithRtxStreamParams(
      "cname", MAKE_VECTOR(kSenderSsrcs), MAKE_VECTOR(kSenderRtxSsrcs));

  EXPECT_TRUE(channel_->AddSendStream(sender_sp));

  static const uint32_t kReceiverSsrcs[] = {3};
  static const uint32_t kReceiverRtxSsrcs[] = {2};

  StreamParams receiver_sp = cricket::CreateSimWithRtxStreamParams(
      "cname", MAKE_VECTOR(kReceiverSsrcs), MAKE_VECTOR(kReceiverRtxSsrcs));
  EXPECT_TRUE(channel_->AddRecvStream(receiver_sp));

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));

  ASSERT_EQ(1u, info.senders.size());
  ASSERT_EQ(1u, info.receivers.size());

  EXPECT_NE(sender_sp.ssrc_groups, receiver_sp.ssrc_groups);
  EXPECT_EQ(sender_sp.ssrc_groups, info.senders[0].ssrc_groups);
  EXPECT_EQ(receiver_sp.ssrc_groups, info.receivers[0].ssrc_groups);
}

TEST_F(WebRtcVideoChannel2Test, MapsReceivedPayloadTypeToCodecName) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  webrtc::VideoReceiveStream::Stats stats;
  cricket::VideoMediaInfo info;

  // Report no codec name before receiving.
  stream->SetStats(stats);
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_STREQ("", info.receivers[0].codec_name.c_str());

  // Report VP8 if we're receiving it.
  stats.current_payload_type = kDefaultVp8PlType;
  stream->SetStats(stats);
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_STREQ(kVp8CodecName, info.receivers[0].codec_name.c_str());

  // Report no codec name for unknown playload types.
  stats.current_payload_type = 3;
  stream->SetStats(stats);
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_STREQ("", info.receivers[0].codec_name.c_str());
}

void WebRtcVideoChannel2Test::TestReceiveUnsignalledSsrcPacket(
    uint8_t payload_type,
    bool expect_created_receive_stream) {
  // Add a RED RTX codec.
  VideoCodec red_rtx_codec =
      VideoCodec::CreateRtxCodec(kRedRtxPayloadType, kDefaultRedPlType);
  recv_parameters_.codecs.push_back(red_rtx_codec);
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));

  ASSERT_EQ(0u, fake_call_->GetVideoReceiveStreams().size());
  const size_t kDataLength = 12;
  uint8_t data[kDataLength];
  memset(data, 0, sizeof(data));

  rtc::Set8(data, 1, payload_type);
  rtc::SetBE32(&data[8], kIncomingUnsignalledSsrc);
  rtc::Buffer packet(data, kDataLength);
  rtc::PacketTime packet_time;
  channel_->OnPacketReceived(&packet, packet_time);

  if (expect_created_receive_stream) {
    EXPECT_EQ(1u, fake_call_->GetVideoReceiveStreams().size())
        << "Should have created a receive stream for payload type: "
        << payload_type;
  } else {
    EXPECT_EQ(0u, fake_call_->GetVideoReceiveStreams().size())
        << "Shouldn't have created a receive stream for payload type: "
        << payload_type;
  }
}

TEST_F(WebRtcVideoChannel2Test, Vp8PacketCreatesUnsignalledStream) {
  TestReceiveUnsignalledSsrcPacket(kDefaultVp8PlType, true);
}

TEST_F(WebRtcVideoChannel2Test, Vp9PacketCreatesUnsignalledStream) {
  TestReceiveUnsignalledSsrcPacket(kDefaultVp9PlType, true);
}

TEST_F(WebRtcVideoChannel2Test, RtxPacketDoesntCreateUnsignalledStream) {
  TestReceiveUnsignalledSsrcPacket(kDefaultRtxVp8PlType, false);
}

TEST_F(WebRtcVideoChannel2Test, UlpfecPacketDoesntCreateUnsignalledStream) {
  TestReceiveUnsignalledSsrcPacket(kDefaultUlpfecType, false);
}

TEST_F(WebRtcVideoChannel2Test, RedRtxPacketDoesntCreateUnsignalledStream) {
  TestReceiveUnsignalledSsrcPacket(kRedRtxPayloadType, false);
}

void WebRtcVideoChannel2Test::TestReceiverLocalSsrcConfiguration(
    bool receiver_first) {
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  const uint32_t kSenderSsrc = 0xC0FFEE;
  const uint32_t kSecondSenderSsrc = 0xBADCAFE;
  const uint32_t kReceiverSsrc = 0x4711;
  const uint32_t kExpectedDefaultReceiverSsrc = 1;

  if (receiver_first) {
    AddRecvStream(StreamParams::CreateLegacy(kReceiverSsrc));
    std::vector<FakeVideoReceiveStream*> receive_streams =
        fake_call_->GetVideoReceiveStreams();
    ASSERT_EQ(1u, receive_streams.size());
    // Default local SSRC when we have no sender.
    EXPECT_EQ(kExpectedDefaultReceiverSsrc,
              receive_streams[0]->GetConfig().rtp.local_ssrc);
  }
  AddSendStream(StreamParams::CreateLegacy(kSenderSsrc));
  if (!receiver_first)
    AddRecvStream(StreamParams::CreateLegacy(kReceiverSsrc));
  std::vector<FakeVideoReceiveStream*> receive_streams =
      fake_call_->GetVideoReceiveStreams();
  ASSERT_EQ(1u, receive_streams.size());
  EXPECT_EQ(kSenderSsrc, receive_streams[0]->GetConfig().rtp.local_ssrc);

  // Removing first sender should fall back to another (in this case the second)
  // local send stream's SSRC.
  AddSendStream(StreamParams::CreateLegacy(kSecondSenderSsrc));
  ASSERT_TRUE(channel_->RemoveSendStream(kSenderSsrc));
  receive_streams =
      fake_call_->GetVideoReceiveStreams();
  ASSERT_EQ(1u, receive_streams.size());
  EXPECT_EQ(kSecondSenderSsrc, receive_streams[0]->GetConfig().rtp.local_ssrc);

  // Removing the last sender should fall back to default local SSRC.
  ASSERT_TRUE(channel_->RemoveSendStream(kSecondSenderSsrc));
  receive_streams =
      fake_call_->GetVideoReceiveStreams();
  ASSERT_EQ(1u, receive_streams.size());
  EXPECT_EQ(kExpectedDefaultReceiverSsrc,
            receive_streams[0]->GetConfig().rtp.local_ssrc);
}

TEST_F(WebRtcVideoChannel2Test, ConfiguresLocalSsrc) {
  TestReceiverLocalSsrcConfiguration(false);
}

TEST_F(WebRtcVideoChannel2Test, ConfiguresLocalSsrcOnExistingReceivers) {
  TestReceiverLocalSsrcConfiguration(true);
}

class WebRtcVideoEngine2SimulcastTest : public testing::Test {};

// Test that if we add a stream with RTX SSRC's, SSRC's get set correctly.
TEST_F(WebRtcVideoEngine2SimulcastTest, DISABLED_TestStreamWithRtx) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

// Test that if we get too few ssrcs are given in AddSendStream(),
// only supported sub-streams will be added.
TEST_F(WebRtcVideoEngine2SimulcastTest, DISABLED_TooFewSimulcastSsrcs) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

// Test that even more than enough ssrcs are given in AddSendStream(),
// only supported sub-streams will be added.
TEST_F(WebRtcVideoEngine2SimulcastTest, DISABLED_MoreThanEnoughSimulcastSscrs) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

// Test that SetSendStreamFormat works well with simulcast.
TEST_F(WebRtcVideoEngine2SimulcastTest,
       DISABLED_SetSendStreamFormatWithSimulcast) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

// Test that simulcast send codec is reset on new video frame size.
TEST_F(WebRtcVideoEngine2SimulcastTest,
       DISABLED_ResetSimulcastSendCodecOnNewFrameSize) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

// Test that simulcast send codec is reset on new portait mode video frame.
TEST_F(WebRtcVideoEngine2SimulcastTest,
       DISABLED_ResetSimulcastSendCodecOnNewPortaitFrame) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

TEST_F(WebRtcVideoEngine2SimulcastTest,
       DISABLED_SetBandwidthInConferenceWithSimulcast) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

// Test that sending screencast frames in conference mode changes
// bitrate.
TEST_F(WebRtcVideoEngine2SimulcastTest,
       DISABLED_SetBandwidthScreencastInConference) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

// Test AddSendStream with simulcast rejects bad StreamParams.
TEST_F(WebRtcVideoEngine2SimulcastTest,
       DISABLED_AddSendStreamWithBadStreamParams) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

// Test AddSendStream with simulcast sets ssrc and cname correctly.
TEST_F(WebRtcVideoEngine2SimulcastTest, DISABLED_AddSendStreamWithSimulcast) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

// Test RemoveSendStream with simulcast.
TEST_F(WebRtcVideoEngine2SimulcastTest,
       DISABLED_RemoveSendStreamWithSimulcast) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

// Test AddSendStream after send codec has already been set will reset
// send codec with simulcast settings.
TEST_F(WebRtcVideoEngine2SimulcastTest,
       DISABLED_AddSimulcastStreamAfterSetSendCodec) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

TEST_F(WebRtcVideoEngine2SimulcastTest, DISABLED_GetStatsWithMultipleSsrcs) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

// Test receiving channel(s) local ssrc is set to the same as the first
// simulcast sending ssrc.
TEST_F(WebRtcVideoEngine2SimulcastTest,
       DISABLED_AddSimulcastStreamAfterCreatingRecvChannels) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

// Test 1:1 call never turn on simulcast.
TEST_F(WebRtcVideoEngine2SimulcastTest, DISABLED_NoSimulcastWith1on1) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

// Test SetOptions with OPT_CONFERENCE flag.
TEST_F(WebRtcVideoEngine2SimulcastTest, DISABLED_SetOptionsWithConferenceMode) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

// Test that two different streams can have different formats.
TEST_F(WebRtcVideoEngine2SimulcastTest,
       DISABLED_MultipleSendStreamsDifferentFormats) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

TEST_F(WebRtcVideoEngine2SimulcastTest, DISABLED_TestAdaptToOutputFormat) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

TEST_F(WebRtcVideoEngine2SimulcastTest,
       DISABLED_TestAdaptWithCpuOveruseObserver) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

// Test that codec is not reset for every frame sent in non-conference and
// non-screencast mode.
TEST_F(WebRtcVideoEngine2SimulcastTest, DISABLED_DontResetCodecOnSendFrame) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

TEST_F(WebRtcVideoEngine2SimulcastTest,
       DISABLED_UseSimulcastAdapterOnVp8OnlyFactory) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

TEST_F(WebRtcVideoEngine2SimulcastTest,
       DISABLED_DontUseSimulcastAdapterOnNonVp8Factory) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

class WebRtcVideoChannel2SimulcastTest : public testing::Test {
 public:
  WebRtcVideoChannel2SimulcastTest() : fake_call_(webrtc::Call::Config()) {}

  void SetUp() override {
    engine_.Init();
    channel_.reset(engine_.CreateChannel(&fake_call_, VideoOptions()));
    last_ssrc_ = 123;
  }

 protected:
  void VerifySimulcastSettings(const VideoCodec& codec,
                               size_t num_configured_streams,
                               size_t expected_num_streams) {
    cricket::VideoSendParameters parameters;
    parameters.codecs.push_back(codec);
    ASSERT_TRUE(channel_->SetSendParameters(parameters));

    std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs3);
    RTC_DCHECK(num_configured_streams <= ssrcs.size());
    ssrcs.resize(num_configured_streams);

    FakeVideoSendStream* stream =
        AddSendStream(CreateSimStreamParams("cname", ssrcs));
    // Send a full-size frame to trigger a stream reconfiguration to use all
    // expected simulcast layers.
    cricket::FakeVideoCapturer capturer;
    EXPECT_TRUE(channel_->SetCapturer(ssrcs.front(), &capturer));
    EXPECT_EQ(cricket::CS_RUNNING, capturer.Start(cricket::VideoFormat(
                                       codec.width, codec.height,
                                       cricket::VideoFormat::FpsToInterval(30),
                                       cricket::FOURCC_I420)));
    channel_->SetSend(true);
    EXPECT_TRUE(capturer.CaptureFrame());

    std::vector<webrtc::VideoStream> video_streams = stream->GetVideoStreams();
    ASSERT_EQ(expected_num_streams, video_streams.size());

    std::vector<webrtc::VideoStream> expected_streams = GetSimulcastConfig(
        num_configured_streams, codec.width, codec.height, 0, kDefaultQpMax,
        codec.framerate != 0 ? codec.framerate : kDefaultFramerate);

    ASSERT_EQ(expected_streams.size(), video_streams.size());

    size_t num_streams = video_streams.size();
    int total_max_bitrate_bps = 0;
    for (size_t i = 0; i < num_streams; ++i) {
      EXPECT_EQ(expected_streams[i].width, video_streams[i].width);
      EXPECT_EQ(expected_streams[i].height, video_streams[i].height);

      EXPECT_GT(video_streams[i].max_framerate, 0);
      EXPECT_EQ(expected_streams[i].max_framerate,
                video_streams[i].max_framerate);

      EXPECT_GT(video_streams[i].min_bitrate_bps, 0);
      EXPECT_EQ(expected_streams[i].min_bitrate_bps,
                video_streams[i].min_bitrate_bps);

      EXPECT_GT(video_streams[i].target_bitrate_bps, 0);
      EXPECT_EQ(expected_streams[i].target_bitrate_bps,
                video_streams[i].target_bitrate_bps);

      EXPECT_GT(video_streams[i].max_bitrate_bps, 0);
      EXPECT_EQ(expected_streams[i].max_bitrate_bps,
                video_streams[i].max_bitrate_bps);

      EXPECT_GT(video_streams[i].max_qp, 0);
      EXPECT_EQ(expected_streams[i].max_qp, video_streams[i].max_qp);

      EXPECT_FALSE(expected_streams[i].temporal_layer_thresholds_bps.empty());
      EXPECT_EQ(expected_streams[i].temporal_layer_thresholds_bps,
                video_streams[i].temporal_layer_thresholds_bps);

      if (i == num_streams - 1) {
        total_max_bitrate_bps += video_streams[i].max_bitrate_bps;
      } else {
        total_max_bitrate_bps += video_streams[i].target_bitrate_bps;
      }
    }
    cricket::VideoMediaInfo info;
    ASSERT_TRUE(channel_->GetStats(&info));
    ASSERT_EQ(1u, info.senders.size());
    EXPECT_EQ(total_max_bitrate_bps, info.senders[0].preferred_bitrate);

    EXPECT_TRUE(channel_->SetCapturer(ssrcs.front(), NULL));
  }

  FakeVideoSendStream* AddSendStream() {
    return AddSendStream(StreamParams::CreateLegacy(last_ssrc_++));
  }

  FakeVideoSendStream* AddSendStream(const StreamParams& sp) {
    size_t num_streams =
        fake_call_.GetVideoSendStreams().size();
    EXPECT_TRUE(channel_->AddSendStream(sp));
    std::vector<FakeVideoSendStream*> streams =
        fake_call_.GetVideoSendStreams();
    EXPECT_EQ(num_streams + 1, streams.size());
    return streams[streams.size() - 1];
  }

  std::vector<FakeVideoSendStream*> GetFakeSendStreams() {
    return fake_call_.GetVideoSendStreams();
  }

  FakeVideoReceiveStream* AddRecvStream() {
    return AddRecvStream(StreamParams::CreateLegacy(last_ssrc_++));
  }

  FakeVideoReceiveStream* AddRecvStream(const StreamParams& sp) {
    size_t num_streams =
        fake_call_.GetVideoReceiveStreams().size();
    EXPECT_TRUE(channel_->AddRecvStream(sp));
    std::vector<FakeVideoReceiveStream*> streams =
        fake_call_.GetVideoReceiveStreams();
    EXPECT_EQ(num_streams + 1, streams.size());
    return streams[streams.size() - 1];
  }

  FakeCall fake_call_;
  WebRtcVideoEngine2 engine_;
  rtc::scoped_ptr<VideoMediaChannel> channel_;
  uint32_t last_ssrc_;
};

TEST_F(WebRtcVideoChannel2SimulcastTest, SetSendCodecsWith2SimulcastStreams) {
  VerifySimulcastSettings(kVp8Codec, 2, 2);
}

TEST_F(WebRtcVideoChannel2SimulcastTest, SetSendCodecsWith3SimulcastStreams) {
  VerifySimulcastSettings(kVp8Codec720p, 3, 3);
}

// Test that we normalize send codec format size in simulcast.
TEST_F(WebRtcVideoChannel2SimulcastTest, SetSendCodecsWithOddSizeInSimulcast) {
  cricket::VideoCodec codec(kVp8Codec270p);
  codec.width += 1;
  codec.height += 1;
  VerifySimulcastSettings(codec, 2, 2);
}

TEST_F(WebRtcVideoChannel2SimulcastTest, DISABLED_SimulcastSend_1280x800) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

TEST_F(WebRtcVideoChannel2SimulcastTest, DISABLED_SimulcastSend_1280x720) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

TEST_F(WebRtcVideoChannel2SimulcastTest, DISABLED_SimulcastSend_960x540) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

TEST_F(WebRtcVideoChannel2SimulcastTest, DISABLED_SimulcastSend_960x600) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

TEST_F(WebRtcVideoChannel2SimulcastTest, DISABLED_SimulcastSend_640x400) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

TEST_F(WebRtcVideoChannel2SimulcastTest, DISABLED_SimulcastSend_640x360) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

TEST_F(WebRtcVideoChannel2SimulcastTest, DISABLED_SimulcastSend_480x300) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

TEST_F(WebRtcVideoChannel2SimulcastTest, DISABLED_SimulcastSend_480x270) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

TEST_F(WebRtcVideoChannel2SimulcastTest, DISABLED_SimulcastSend_320x200) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

TEST_F(WebRtcVideoChannel2SimulcastTest, DISABLED_SimulcastSend_320x180) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

// Test simulcast streams are decodeable with expected sizes.
TEST_F(WebRtcVideoChannel2SimulcastTest, DISABLED_SimulcastStreams) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

// Simulcast and resolution resizing should be turned off when screencasting
// but not otherwise.
TEST_F(WebRtcVideoChannel2SimulcastTest, DISABLED_ScreencastRendering) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

// Ensures that the correct settings are applied to the codec when single
// temporal layer screencasting is enabled, and that the correct simulcast
// settings are reapplied when disabling screencasting.
TEST_F(WebRtcVideoChannel2SimulcastTest,
       DISABLED_OneTemporalLayerScreencastSettings) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

// Ensures that the correct settings are applied to the codec when two temporal
// layer screencasting is enabled, and that the correct simulcast settings are
// reapplied when disabling screencasting.
TEST_F(WebRtcVideoChannel2SimulcastTest,
       DISABLED_TwoTemporalLayerScreencastSettings) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

}  // namespace cricket
