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

#include <map>
#include <vector>

#include "webrtc/base/gunit.h"
#include "webrtc/base/stringutils.h"
#include "talk/media/base/testutils.h"
#include "talk/media/base/videoengine_unittest.h"
#include "talk/media/webrtc/webrtcvideoengine2.h"
#include "talk/media/webrtc/webrtcvideoengine2_unittest.h"
#include "talk/media/webrtc/webrtcvideochannelfactory.h"

namespace {
static const cricket::VideoCodec kVp8Codec720p(100, "VP8", 1280, 720, 30, 0);
static const cricket::VideoCodec kVp8Codec360p(100, "VP8", 640, 360, 30, 0);
static const cricket::VideoCodec kVp8Codec270p(100, "VP8", 480, 270, 30, 0);
static const cricket::VideoCodec kVp8Codec180p(100, "VP8", 320, 180, 30, 0);

static const cricket::VideoCodec kVp8Codec(100, "VP8", 640, 400, 30, 0);
static const cricket::VideoCodec kVp9Codec(101, "VP9", 640, 400, 30, 0);
static const cricket::VideoCodec kRedCodec(116, "red", 0, 0, 0, 0);
static const cricket::VideoCodec kUlpfecCodec(117, "ulpfec", 0, 0, 0, 0);

static const uint32 kSsrcs1[] = {1};
static const uint32 kRtxSsrcs1[] = {4};
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
      cricket::kRtcpFbParamCcm, cricket::kRtcpFbCcmParamFir)));
}

}  // namespace

namespace cricket {
FakeVideoSendStream::FakeVideoSendStream(
    const webrtc::VideoSendStream::Config& config,
    const std::vector<webrtc::VideoStream>& video_streams,
    const void* encoder_settings)
    : sending_(false),
      config_(config),
      codec_settings_set_(false) {
  assert(config.encoder_settings.encoder != NULL);
  ReconfigureVideoEncoder(video_streams, encoder_settings);
}

webrtc::VideoSendStream::Config FakeVideoSendStream::GetConfig() {
  return config_;
}

std::vector<webrtc::VideoStream> FakeVideoSendStream::GetVideoStreams() {
  return video_streams_;
}

bool FakeVideoSendStream::IsSending() const {
  return sending_;
}

bool FakeVideoSendStream::GetVp8Settings(
    webrtc::VideoCodecVP8* settings) const {
  if (!codec_settings_set_) {
    return false;
  }

  *settings = vp8_settings_;
  return true;
}

webrtc::VideoSendStream::Stats FakeVideoSendStream::GetStats() const {
  return webrtc::VideoSendStream::Stats();
}

bool FakeVideoSendStream::ReconfigureVideoEncoder(
    const std::vector<webrtc::VideoStream>& streams,
    const void* encoder_specific) {
  video_streams_ = streams;
  if (encoder_specific != NULL) {
    assert(config_.encoder_settings.payload_name == "VP8");
    vp8_settings_ =
        *reinterpret_cast<const webrtc::VideoCodecVP8*>(encoder_specific);
  }
  codec_settings_set_ = encoder_specific != NULL;
  return true;
}

webrtc::VideoSendStreamInput* FakeVideoSendStream::Input() {
  // TODO(pbos): Fix.
  return NULL;
}

void FakeVideoSendStream::Start() {
  sending_ = true;
}

void FakeVideoSendStream::Stop() {
  sending_ = false;
}

FakeVideoReceiveStream::FakeVideoReceiveStream(
    const webrtc::VideoReceiveStream::Config& config)
    : config_(config), receiving_(false) {
}

webrtc::VideoReceiveStream::Config FakeVideoReceiveStream::GetConfig() {
  return config_;
}

bool FakeVideoReceiveStream::IsReceiving() const {
  return receiving_;
}

webrtc::VideoReceiveStream::Stats FakeVideoReceiveStream::GetStats() const {
  return webrtc::VideoReceiveStream::Stats();
}

void FakeVideoReceiveStream::Start() {
  receiving_ = true;
}

void FakeVideoReceiveStream::Stop() {
  receiving_ = false;
}

void FakeVideoReceiveStream::GetCurrentReceiveCodec(webrtc::VideoCodec* codec) {
}

FakeCall::FakeCall() { SetVideoCodecs(GetDefaultVideoCodecs()); }

FakeCall::~FakeCall() {
  EXPECT_EQ(0u, video_send_streams_.size());
  EXPECT_EQ(0u, video_receive_streams_.size());
}

void FakeCall::SetVideoCodecs(const std::vector<webrtc::VideoCodec> codecs) {
  codecs_ = codecs;
}

std::vector<FakeVideoSendStream*> FakeCall::GetVideoSendStreams() {
  return video_send_streams_;
}

std::vector<FakeVideoReceiveStream*> FakeCall::GetVideoReceiveStreams() {
  return video_receive_streams_;
}

webrtc::VideoCodec FakeCall::GetEmptyVideoCodec() {
  webrtc::VideoCodec codec;
  codec.minBitrate = 300;
  codec.startBitrate = 800;
  codec.maxBitrate = 1500;
  codec.maxFramerate = 10;
  codec.width = 640;
  codec.height = 480;
  codec.qpMax = 56;

  return codec;
}

webrtc::VideoCodec FakeCall::GetVideoCodecVp8() {
  webrtc::VideoCodec vp8_codec = GetEmptyVideoCodec();
  vp8_codec.codecType = webrtc::kVideoCodecVP8;
  rtc::strcpyn(vp8_codec.plName, ARRAY_SIZE(vp8_codec.plName),
      kVp8Codec.name.c_str());
  vp8_codec.plType = kVp8Codec.id;

  return vp8_codec;
}

webrtc::VideoCodec FakeCall::GetVideoCodecVp9() {
  webrtc::VideoCodec vp9_codec = GetEmptyVideoCodec();
  // TODO(pbos): Add a correct codecType when webrtc has one.
  vp9_codec.codecType = webrtc::kVideoCodecVP8;
  rtc::strcpyn(vp9_codec.plName, ARRAY_SIZE(vp9_codec.plName),
      kVp9Codec.name.c_str());
  vp9_codec.plType = kVp9Codec.id;

  return vp9_codec;
}

std::vector<webrtc::VideoCodec> FakeCall::GetDefaultVideoCodecs() {
  std::vector<webrtc::VideoCodec> codecs;
  codecs.push_back(GetVideoCodecVp8());
  //    codecs.push_back(GetVideoCodecVp9());

  return codecs;
}

webrtc::VideoSendStream* FakeCall::CreateVideoSendStream(
    const webrtc::VideoSendStream::Config& config,
    const std::vector<webrtc::VideoStream>& video_streams,
    const void* encoder_settings) {
  FakeVideoSendStream* fake_stream =
      new FakeVideoSendStream(config, video_streams, encoder_settings);
  video_send_streams_.push_back(fake_stream);
  return fake_stream;
}

void FakeCall::DestroyVideoSendStream(webrtc::VideoSendStream* send_stream) {
  FakeVideoSendStream* fake_stream =
      static_cast<FakeVideoSendStream*>(send_stream);
  for (size_t i = 0; i < video_send_streams_.size(); ++i) {
    if (video_send_streams_[i] == fake_stream) {
      delete video_send_streams_[i];
      video_send_streams_.erase(video_send_streams_.begin() + i);
      return;
    }
  }
  ADD_FAILURE() << "DestroyVideoSendStream called with unknown paramter.";
}

webrtc::VideoReceiveStream* FakeCall::CreateVideoReceiveStream(
    const webrtc::VideoReceiveStream::Config& config) {
  video_receive_streams_.push_back(new FakeVideoReceiveStream(config));
  return video_receive_streams_[video_receive_streams_.size() - 1];
}

void FakeCall::DestroyVideoReceiveStream(
    webrtc::VideoReceiveStream* receive_stream) {
  FakeVideoReceiveStream* fake_stream =
      static_cast<FakeVideoReceiveStream*>(receive_stream);
  for (size_t i = 0; i < video_receive_streams_.size(); ++i) {
    if (video_receive_streams_[i] == fake_stream) {
      delete video_receive_streams_[i];
      video_receive_streams_.erase(video_receive_streams_.begin() + i);
      return;
    }
  }
  ADD_FAILURE() << "DestroyVideoReceiveStream called with unknown paramter.";
}

webrtc::PacketReceiver* FakeCall::Receiver() {
  // TODO(pbos): Fix this.
  return NULL;
}

uint32_t FakeCall::SendBitrateEstimate() {
  return 0;
}

uint32_t FakeCall::ReceiveBitrateEstimate() {
  return 0;
}

FakeWebRtcVideoChannel2::FakeWebRtcVideoChannel2(
    FakeCall* call,
    WebRtcVideoEngine2* engine,
    VoiceMediaChannel* voice_channel)
    : WebRtcVideoChannel2(call, engine, engine->GetVideoEncoderFactory()),
      fake_call_(call),
      voice_channel_(voice_channel) {
}

FakeWebRtcVideoChannel2::~FakeWebRtcVideoChannel2() {
}

VoiceMediaChannel* FakeWebRtcVideoChannel2::GetVoiceChannel() {
  return voice_channel_;
}
FakeCall* FakeWebRtcVideoChannel2::GetFakeCall() {
  return fake_call_;
}

FakeWebRtcVideoChannel2* FakeWebRtcVideoMediaChannelFactory::GetFakeChannel(
    VideoMediaChannel* channel) {
  return channel_map_[channel];
}

WebRtcVideoChannel2* FakeWebRtcVideoMediaChannelFactory::Create(
    WebRtcVideoEngine2* engine,
    VoiceMediaChannel* voice_channel) {
  FakeWebRtcVideoChannel2* channel =
      new FakeWebRtcVideoChannel2(new FakeCall(), engine, voice_channel);
  channel_map_[channel] = channel;
  return channel;
}

class WebRtcVideoEngine2Test : public testing::Test {
 public:
  WebRtcVideoEngine2Test() : engine_(&factory_) {
    std::vector<VideoCodec> engine_codecs = engine_.codecs();
    assert(!engine_codecs.empty());
    bool codec_set = false;
    for (size_t i = 0; i < engine_codecs.size(); ++i) {
      if (engine_codecs[i].name == "red") {
        default_red_codec_ = engine_codecs[i];
      } else if (engine_codecs[i].name == "ulpfec") {
        default_ulpfec_codec_ = engine_codecs[i];
      } else if (engine_codecs[i].name == "rtx") {
        default_rtx_codec_ = engine_codecs[i];
      } else if (!codec_set) {
        default_codec_ = engine_codecs[i];
        codec_set = true;
      }
    }

    assert(codec_set);
  }

 protected:
  FakeWebRtcVideoMediaChannelFactory factory_;
  WebRtcVideoEngine2 engine_;
  VideoCodec default_codec_;
  VideoCodec default_red_codec_;
  VideoCodec default_ulpfec_codec_;
  VideoCodec default_rtx_codec_;
};

TEST_F(WebRtcVideoEngine2Test, CreateChannel) {
  rtc::scoped_ptr<VideoMediaChannel> channel(engine_.CreateChannel(NULL));
  ASSERT_TRUE(channel.get() != NULL) << "Could not create channel.";
  EXPECT_TRUE(factory_.GetFakeChannel(channel.get()) != NULL)
      << "Channel not created through factory.";
}

TEST_F(WebRtcVideoEngine2Test, CreateChannelWithVoiceEngine) {
  VoiceMediaChannel* voice_channel = reinterpret_cast<VoiceMediaChannel*>(0x42);
  rtc::scoped_ptr<VideoMediaChannel> channel(
      engine_.CreateChannel(voice_channel));
  ASSERT_TRUE(channel.get() != NULL) << "Could not create channel.";

  FakeWebRtcVideoChannel2* fake_channel =
      factory_.GetFakeChannel(channel.get());
  ASSERT_TRUE(fake_channel != NULL) << "Channel not created through factory.";

  EXPECT_TRUE(fake_channel->GetVoiceChannel() != NULL)
      << "VoiceChannel not set.";
  EXPECT_EQ(voice_channel, fake_channel->GetVoiceChannel())
      << "Different VoiceChannel set than the provided one.";
}

TEST_F(WebRtcVideoEngine2Test, FindCodec) {
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
  std::vector<RtpHeaderExtension> extensions = engine_.rtp_header_extensions();
  ASSERT_FALSE(extensions.empty());
  for (size_t i = 0; i < extensions.size(); ++i) {
    if (extensions[i].uri == kRtpTimestampOffsetHeaderExtension) {
      EXPECT_EQ(kRtpTimestampOffsetHeaderExtensionDefaultId, extensions[i].id);
      return;
    }
  }
  FAIL() << "Timestamp offset extension not in header-extension list.";
}

TEST_F(WebRtcVideoEngine2Test, SupportsAbsoluteSenderTimeHeaderExtension) {
  std::vector<RtpHeaderExtension> extensions = engine_.rtp_header_extensions();
  ASSERT_FALSE(extensions.empty());
  for (size_t i = 0; i < extensions.size(); ++i) {
    if (extensions[i].uri == kRtpAbsoluteSenderTimeHeaderExtension) {
      EXPECT_EQ(kRtpAbsoluteSenderTimeHeaderExtensionDefaultId,
                extensions[i].id);
      return;
    }
  }
  FAIL() << "Absolute Sender Time extension not in header-extension list.";
}

TEST_F(WebRtcVideoEngine2Test, SetSendFailsBeforeSettingCodecs) {
  rtc::scoped_ptr<VideoMediaChannel> channel(engine_.CreateChannel(NULL));

  EXPECT_TRUE(channel->AddSendStream(StreamParams::CreateLegacy(123)));

  EXPECT_FALSE(channel->SetSend(true))
      << "Channel should not start without codecs.";
  EXPECT_TRUE(channel->SetSend(false))
      << "Channel should be stoppable even without set codecs.";
}

class WebRtcVideoEngine2BaseTest
    : public VideoEngineTest<cricket::WebRtcVideoEngine2> {
 protected:
  typedef VideoEngineTest<cricket::WebRtcVideoEngine2> Base;
};

#define WEBRTC_ENGINE_BASE_TEST(test) \
  TEST_F(WebRtcVideoEngine2BaseTest, test) { Base::test##Body(); }

WEBRTC_ENGINE_BASE_TEST(ConstrainNewCodec2);

class WebRtcVideoChannel2BaseTest
    : public VideoMediaChannelTest<WebRtcVideoEngine2, WebRtcVideoChannel2> {
 protected:
  virtual cricket::VideoCodec DefaultCodec() OVERRIDE { return kVp8Codec; }
  typedef VideoMediaChannelTest<WebRtcVideoEngine2, WebRtcVideoChannel2> Base;
};

#define WEBRTC_BASE_TEST(test) \
  TEST_F(WebRtcVideoChannel2BaseTest, test) { Base::test(); }

#define WEBRTC_DISABLED_BASE_TEST(test) \
  TEST_F(WebRtcVideoChannel2BaseTest, DISABLED_ ## test) { Base::test(); }

// TODO(pbos): Fix WebRtcVideoEngine2BaseTest, where we want CheckCoInitialize.
#if 0
// TODO(juberti): Figure out why ViE is munging the COM refcount.
#ifdef WIN32
WEBRTC_DISABLED_BASE_TEST(CheckCoInitialize) {
  Base::CheckCoInitialize();
}
#endif
#endif

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

WEBRTC_BASE_TEST(MuteStream);

WEBRTC_BASE_TEST(MultipleSendStreams);

// TODO(juberti): Restore this test once we support sending 0 fps.
WEBRTC_DISABLED_BASE_TEST(AdaptDropAllFrames);
// TODO(juberti): Understand why we get decode errors on this test.
WEBRTC_DISABLED_BASE_TEST(AdaptFramerate);

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

WEBRTC_BASE_TEST(SendManyResizeOnce);

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

class WebRtcVideoChannel2Test : public WebRtcVideoEngine2Test {
 public:
  virtual void SetUp() OVERRIDE {
    channel_.reset(engine_.CreateChannel(NULL));
    fake_channel_ = factory_.GetFakeChannel(channel_.get());
    last_ssrc_ = 123;
    ASSERT_TRUE(fake_channel_ != NULL)
        << "Channel not created through factory.";
    EXPECT_TRUE(fake_channel_->SetSendCodecs(engine_.codecs()));
  }

 protected:
  FakeVideoSendStream* AddSendStream() {
    return AddSendStream(StreamParams::CreateLegacy(last_ssrc_++));
  }

  FakeVideoSendStream* AddSendStream(const StreamParams& sp) {
    size_t num_streams =
        fake_channel_->GetFakeCall()->GetVideoSendStreams().size();
    EXPECT_TRUE(channel_->AddSendStream(sp));
    std::vector<FakeVideoSendStream*> streams =
        fake_channel_->GetFakeCall()->GetVideoSendStreams();
    EXPECT_EQ(num_streams + 1, streams.size());
    return streams[streams.size() - 1];
  }

  std::vector<FakeVideoSendStream*> GetFakeSendStreams() {
    return fake_channel_->GetFakeCall()->GetVideoSendStreams();
  }

  FakeVideoReceiveStream* AddRecvStream() {
    return AddRecvStream(StreamParams::CreateLegacy(last_ssrc_++));
  }

  FakeVideoReceiveStream* AddRecvStream(const StreamParams& sp) {
    size_t num_streams =
        fake_channel_->GetFakeCall()->GetVideoReceiveStreams().size();
    EXPECT_TRUE(channel_->AddRecvStream(sp));
    std::vector<FakeVideoReceiveStream*> streams =
        fake_channel_->GetFakeCall()->GetVideoReceiveStreams();
    EXPECT_EQ(num_streams + 1, streams.size());
    return streams[streams.size() - 1];
  }

  void SetSendCodecsShouldWorkForBitrates(const char* min_bitrate,
                                          const char* max_bitrate) {
    std::vector<VideoCodec> codecs;
    codecs.push_back(kVp8Codec);
    codecs[0].params[kCodecParamMinBitrate] = min_bitrate;
    codecs[0].params[kCodecParamMaxBitrate] = max_bitrate;
    EXPECT_TRUE(channel_->SetSendCodecs(codecs));

    FakeVideoSendStream* stream = AddSendStream();

    std::vector<webrtc::VideoStream> video_streams = stream->GetVideoStreams();
    ASSERT_EQ(1u, video_streams.size());
    EXPECT_EQ(atoi(min_bitrate), video_streams.back().min_bitrate_bps / 1000);
    EXPECT_EQ(atoi(max_bitrate), video_streams.back().max_bitrate_bps / 1000);

    VideoCodec codec;
    EXPECT_TRUE(channel_->GetSendCodec(&codec));
    EXPECT_EQ(min_bitrate, codec.params[kCodecParamMinBitrate]);
    EXPECT_EQ(max_bitrate, codec.params[kCodecParamMaxBitrate]);
  }

  void TestSetSendRtpHeaderExtensions(const std::string& cricket_ext,
                                      const std::string& webrtc_ext) {
    FakeCall* call = fake_channel_->GetFakeCall();
    // Enable extension.
    const int id = 1;
    std::vector<cricket::RtpHeaderExtension> extensions;
    extensions.push_back(cricket::RtpHeaderExtension(cricket_ext, id));
    EXPECT_TRUE(channel_->SetSendRtpHeaderExtensions(extensions));

    FakeVideoSendStream* send_stream =
        AddSendStream(cricket::StreamParams::CreateLegacy(123));

    // Verify the send extension id.
    ASSERT_EQ(1u, send_stream->GetConfig().rtp.extensions.size());
    EXPECT_EQ(id, send_stream->GetConfig().rtp.extensions[0].id);
    EXPECT_EQ(webrtc_ext, send_stream->GetConfig().rtp.extensions[0].name);
    // Verify call with same set of extensions returns true.
    EXPECT_TRUE(channel_->SetSendRtpHeaderExtensions(extensions));
    // Verify that SetSendRtpHeaderExtensions doesn't implicitly add them for
    // receivers.
    EXPECT_TRUE(AddRecvStream(cricket::StreamParams::CreateLegacy(123))
                    ->GetConfig()
                    .rtp.extensions.empty());

    // Verify that existing RTP header extensions can be removed.
    std::vector<cricket::RtpHeaderExtension> empty_extensions;
    EXPECT_TRUE(channel_->SetSendRtpHeaderExtensions(empty_extensions));
    ASSERT_EQ(1u, call->GetVideoSendStreams().size());
    send_stream = call->GetVideoSendStreams()[0];
    EXPECT_TRUE(send_stream->GetConfig().rtp.extensions.empty());

    // Verify that adding receive RTP header extensions adds them for existing
    // streams.
    EXPECT_TRUE(channel_->SetSendRtpHeaderExtensions(extensions));
    send_stream = call->GetVideoSendStreams()[0];
    ASSERT_EQ(1u, send_stream->GetConfig().rtp.extensions.size());
    EXPECT_EQ(id, send_stream->GetConfig().rtp.extensions[0].id);
    EXPECT_EQ(webrtc_ext, send_stream->GetConfig().rtp.extensions[0].name);
  }

  void TestSetRecvRtpHeaderExtensions(const std::string& cricket_ext,
                                      const std::string& webrtc_ext) {
    FakeCall* call = fake_channel_->GetFakeCall();
    // Enable extension.
    const int id = 1;
    std::vector<cricket::RtpHeaderExtension> extensions;
    extensions.push_back(cricket::RtpHeaderExtension(cricket_ext, id));
    EXPECT_TRUE(channel_->SetRecvRtpHeaderExtensions(extensions));

    FakeVideoReceiveStream* recv_stream =
        AddRecvStream(cricket::StreamParams::CreateLegacy(123));

    // Verify the recv extension id.
    ASSERT_EQ(1u, recv_stream->GetConfig().rtp.extensions.size());
    EXPECT_EQ(id, recv_stream->GetConfig().rtp.extensions[0].id);
    EXPECT_EQ(webrtc_ext, recv_stream->GetConfig().rtp.extensions[0].name);
    // Verify call with same set of extensions returns true.
    EXPECT_TRUE(channel_->SetRecvRtpHeaderExtensions(extensions));

    // Verify that SetRecvRtpHeaderExtensions doesn't implicitly add them for
    // senders.
    EXPECT_TRUE(AddSendStream(cricket::StreamParams::CreateLegacy(123))
                    ->GetConfig()
                    .rtp.extensions.empty());

    // Verify that existing RTP header extensions can be removed.
    std::vector<cricket::RtpHeaderExtension> empty_extensions;
    EXPECT_TRUE(channel_->SetRecvRtpHeaderExtensions(empty_extensions));
    ASSERT_EQ(1u, call->GetVideoReceiveStreams().size());
    recv_stream = call->GetVideoReceiveStreams()[0];
    EXPECT_TRUE(recv_stream->GetConfig().rtp.extensions.empty());

    // Verify that adding receive RTP header extensions adds them for existing
    // streams.
    EXPECT_TRUE(channel_->SetRecvRtpHeaderExtensions(extensions));
    recv_stream = call->GetVideoReceiveStreams()[0];
    ASSERT_EQ(1u, recv_stream->GetConfig().rtp.extensions.size());
    EXPECT_EQ(id, recv_stream->GetConfig().rtp.extensions[0].id);
    EXPECT_EQ(webrtc_ext, recv_stream->GetConfig().rtp.extensions[0].name);
  }

  rtc::scoped_ptr<VideoMediaChannel> channel_;
  FakeWebRtcVideoChannel2* fake_channel_;
  uint32 last_ssrc_;
};

TEST_F(WebRtcVideoChannel2Test, DISABLED_MaxBitrateResetsWithConferenceMode) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_StartSendBitrate) {
  // TODO(pbos): Is this test testing vie_ ? this is confusing. No API to set
  // start send bitrate from outside? Add defaults here that should be kept?
  std::vector<cricket::VideoCodec> codec_list;
  codec_list.push_back(kVp8Codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codec_list));
  const unsigned int kVideoMinSendBitrateKbps = 50;
  const unsigned int kVideoTargetSendBitrateKbps = 300;
  const unsigned int kVideoMaxSendBitrateKbps = 2000;
  FakeVideoSendStream* stream = AddSendStream();
  std::vector<webrtc::VideoStream> video_streams = stream->GetVideoStreams();
  ASSERT_EQ(1u, video_streams.size());
  EXPECT_EQ(kVideoMinSendBitrateKbps,
            video_streams.back().min_bitrate_bps / 1000);
  EXPECT_EQ(kVideoTargetSendBitrateKbps,
            video_streams.back().target_bitrate_bps / 1000);
  EXPECT_EQ(kVideoMaxSendBitrateKbps,
            video_streams.back().max_bitrate_bps / 1000);
#if 0
  // TODO(pbos): un-#if
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
#endif
}

TEST_F(WebRtcVideoChannel2Test, RecvStreamWithSimAndRtx) {
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));
  EXPECT_TRUE(channel_->SetSend(true));
  cricket::VideoOptions options;
  options.conference_mode.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));

  // Send side.
  const std::vector<uint32> ssrcs = MAKE_VECTOR(kSsrcs1);
  const std::vector<uint32> rtx_ssrcs = MAKE_VECTOR(kRtxSsrcs1);
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

TEST_F(WebRtcVideoChannel2Test,
    SetSendRtpHeaderExtensionsExcludeUnsupportedExtensions) {
  const int kUnsupportedId = 1;
  const int kTOffsetId = 2;

  std::vector<cricket::RtpHeaderExtension> extensions;
  extensions.push_back(cricket::RtpHeaderExtension(
      kUnsupportedExtensionName, kUnsupportedId));
  extensions.push_back(cricket::RtpHeaderExtension(
      webrtc::RtpExtension::kTOffset, kTOffsetId));
  EXPECT_TRUE(channel_->SetSendRtpHeaderExtensions(extensions));
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

  std::vector<cricket::RtpHeaderExtension> extensions;
  extensions.push_back(cricket::RtpHeaderExtension(
      kUnsupportedExtensionName, kUnsupportedId));
  extensions.push_back(cricket::RtpHeaderExtension(
      webrtc::RtpExtension::kTOffset, kTOffsetId));
  EXPECT_TRUE(channel_->SetRecvRtpHeaderExtensions(extensions));
  FakeVideoReceiveStream* recv_stream =
      AddRecvStream(cricket::StreamParams::CreateLegacy(123));

  // Only timestamp offset extension is set to receive stream,
  // unsupported rtp extension is ignored.
  ASSERT_EQ(1u, recv_stream->GetConfig().rtp.extensions.size());
  EXPECT_STREQ(webrtc::RtpExtension::kTOffset,
      recv_stream->GetConfig().rtp.extensions[0].name.c_str());
}

TEST_F(WebRtcVideoChannel2Test,
    SetSendRtpHeaderExtensionsRejectsIncorrectIds) {
  const size_t kNumIncorrectIds = 4;
  const int kIncorrectIds[kNumIncorrectIds] = {-2, -1, 15, 16};
  for (size_t i = 0; i < kNumIncorrectIds; ++i) {
    std::vector<cricket::RtpHeaderExtension> extensions;
    extensions.push_back(cricket::RtpHeaderExtension(
        webrtc::RtpExtension::kTOffset, kIncorrectIds[i]));
    EXPECT_FALSE(channel_->SetSendRtpHeaderExtensions(extensions))
        << "Bad extension id '" << kIncorrectIds[i] << "' accepted.";
  }
}

TEST_F(WebRtcVideoChannel2Test,
    SetRecvRtpHeaderExtensionsRejectsIncorrectIds) {
  const size_t kNumIncorrectIds = 4;
  const int kIncorrectIds[kNumIncorrectIds] = {-2, -1, 15, 16};
  for (size_t i = 0; i < kNumIncorrectIds; ++i) {
    std::vector<cricket::RtpHeaderExtension> extensions;
    extensions.push_back(cricket::RtpHeaderExtension(
        webrtc::RtpExtension::kTOffset, kIncorrectIds[i]));
    EXPECT_FALSE(channel_->SetRecvRtpHeaderExtensions(extensions))
        << "Bad extension id '" << kIncorrectIds[i] << "' accepted.";
  }
}

TEST_F(WebRtcVideoChannel2Test,
    SetSendRtpHeaderExtensionsRejectsDuplicateIds) {
  const int id = 1;
  std::vector<cricket::RtpHeaderExtension> extensions;
  extensions.push_back(cricket::RtpHeaderExtension(
      webrtc::RtpExtension::kTOffset, id));
  extensions.push_back(cricket::RtpHeaderExtension(
      kRtpAbsoluteSenderTimeHeaderExtension, id));
  EXPECT_FALSE(channel_->SetSendRtpHeaderExtensions(extensions));

  // Duplicate entries are also not supported.
  extensions.clear();
  extensions.push_back(cricket::RtpHeaderExtension(
      webrtc::RtpExtension::kTOffset, id));
  extensions.push_back(extensions.back());
  EXPECT_FALSE(channel_->SetSendRtpHeaderExtensions(extensions));
}

TEST_F(WebRtcVideoChannel2Test,
    SetRecvRtpHeaderExtensionsRejectsDuplicateIds) {
  const int id = 1;
  std::vector<cricket::RtpHeaderExtension> extensions;
  extensions.push_back(cricket::RtpHeaderExtension(
      webrtc::RtpExtension::kTOffset, id));
  extensions.push_back(cricket::RtpHeaderExtension(
      kRtpAbsoluteSenderTimeHeaderExtension, id));
  EXPECT_FALSE(channel_->SetRecvRtpHeaderExtensions(extensions));

  // Duplicate entries are also not supported.
  extensions.clear();
  extensions.push_back(cricket::RtpHeaderExtension(
      webrtc::RtpExtension::kTOffset, id));
  extensions.push_back(extensions.back());
  EXPECT_FALSE(channel_->SetRecvRtpHeaderExtensions(extensions));
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
  EXPECT_EQ(1u, fake_channel_->GetFakeCall()->GetVideoReceiveStreams().size());
}

TEST_F(WebRtcVideoChannel2Test, RembIsEnabledByDefault) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  EXPECT_TRUE(stream->GetConfig().rtp.remb);
}

TEST_F(WebRtcVideoChannel2Test, RembCanBeEnabledAndDisabled) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  EXPECT_TRUE(stream->GetConfig().rtp.remb);

  // Verify that REMB is turned off when codecs without REMB are set.
  std::vector<VideoCodec> codecs;
  codecs.push_back(kVp8Codec);
  EXPECT_TRUE(codecs[0].feedback_params.params().empty());
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));
  stream = fake_channel_->GetFakeCall()->GetVideoReceiveStreams()[0];
  EXPECT_FALSE(stream->GetConfig().rtp.remb);

  // Verify that REMB is turned on when setting default codecs since the
  // default codecs have REMB enabled.
  EXPECT_TRUE(channel_->SetRecvCodecs(engine_.codecs()));
  stream = fake_channel_->GetFakeCall()->GetVideoReceiveStreams()[0];
  EXPECT_TRUE(stream->GetConfig().rtp.remb);
}

TEST_F(WebRtcVideoChannel2Test, NackIsEnabledByDefault) {
  VerifyCodecHasDefaultFeedbackParams(default_codec_);

  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));
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

TEST_F(WebRtcVideoChannel2Test, NackCanBeDisabled) {
  std::vector<VideoCodec> codecs;
  codecs.push_back(kVp8Codec);

  // Send side.
  ASSERT_TRUE(channel_->SetSendCodecs(codecs));
  FakeVideoSendStream* send_stream =
      AddSendStream(cricket::StreamParams::CreateLegacy(1));
  EXPECT_EQ(0, send_stream->GetConfig().rtp.nack.rtp_history_ms);

  // Receiver side.
  ASSERT_TRUE(channel_->SetRecvCodecs(codecs));
  FakeVideoReceiveStream* recv_stream =
      AddRecvStream(cricket::StreamParams::CreateLegacy(1));
  EXPECT_EQ(0, recv_stream->GetConfig().rtp.nack.rtp_history_ms);
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

TEST_F(WebRtcVideoChannel2Test, DISABLED_SetRender) {
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

TEST_F(WebRtcVideoChannel2Test, DISABLED_SetBandwidthScreencast) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
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
  VideoOptions options;
  options.suspend_below_min_bitrate.Set(true);
  channel_->SetOptions(options);

  FakeVideoSendStream* stream = AddSendStream();
  EXPECT_TRUE(stream->GetConfig().suspend_below_min_bitrate);

  options.suspend_below_min_bitrate.Set(false);
  channel_->SetOptions(options);

  stream = fake_channel_->GetFakeCall()->GetVideoSendStreams()[0];
  EXPECT_FALSE(stream->GetConfig().suspend_below_min_bitrate);
}

TEST_F(WebRtcVideoChannel2Test, RedundantPayloadsDisabledByDefault) {
  const std::vector<uint32> ssrcs = MAKE_VECTOR(kSsrcs1);
  const std::vector<uint32> rtx_ssrcs = MAKE_VECTOR(kRtxSsrcs1);
  FakeVideoSendStream* stream = AddSendStream(
      cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs));
  EXPECT_FALSE(stream->GetConfig().rtp.rtx.pad_with_redundant_payloads);
}

TEST_F(WebRtcVideoChannel2Test, SetOptionsWithPayloadPadding) {
  VideoOptions options;
  options.use_payload_padding.Set(true);
  channel_->SetOptions(options);

  const std::vector<uint32> ssrcs = MAKE_VECTOR(kSsrcs1);
  const std::vector<uint32> rtx_ssrcs = MAKE_VECTOR(kRtxSsrcs1);
  FakeVideoSendStream* stream = AddSendStream(
      cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs));
  EXPECT_TRUE(stream->GetConfig().rtp.rtx.pad_with_redundant_payloads);

  options.use_payload_padding.Set(false);
  channel_->SetOptions(options);

  stream = fake_channel_->GetFakeCall()->GetVideoSendStreams()[0];
  EXPECT_FALSE(stream->GetConfig().rtp.rtx.pad_with_redundant_payloads);
}

TEST_F(WebRtcVideoChannel2Test, Vp8DenoisingEnabledByDefault) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoCodecVP8 vp8_settings;
  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  EXPECT_TRUE(vp8_settings.denoisingOn);
}

TEST_F(WebRtcVideoChannel2Test, SetOptionsWithDenoising) {
  VideoOptions options;
  options.video_noise_reduction.Set(false);
  channel_->SetOptions(options);

  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoCodecVP8 vp8_settings;
  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  EXPECT_FALSE(vp8_settings.denoisingOn);

  options.video_noise_reduction.Set(true);
  channel_->SetOptions(options);

  stream = fake_channel_->GetFakeCall()->GetVideoSendStreams()[0];
  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  EXPECT_TRUE(vp8_settings.denoisingOn);
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_MultipleSendStreamsWithOneCapturer) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_SendReceiveBitratesStats) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_TestSetAdaptInputToCpuUsage) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_TestSetCpuThreshold) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_TestSetInvalidCpuThreshold) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_WebRtcShouldLog) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_WebRtcShouldNotLog) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, SetDefaultSendCodecs) {
  ASSERT_TRUE(channel_->SetSendCodecs(engine_.codecs()));

  VideoCodec codec;
  EXPECT_TRUE(channel_->GetSendCodec(&codec));
  EXPECT_TRUE(codec.Matches(engine_.codecs()[0]));

  // Using a RTX setup to verify that the default RTX payload type is good.
  const std::vector<uint32> ssrcs = MAKE_VECTOR(kSsrcs1);
  const std::vector<uint32> rtx_ssrcs = MAKE_VECTOR(kRtxSsrcs1);
  FakeVideoSendStream* stream = AddSendStream(
      cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs));
  webrtc::VideoSendStream::Config config = stream->GetConfig();

  // Make sure NACK and FEC are enabled on the correct payload types.
  EXPECT_EQ(1000, config.rtp.nack.rtp_history_ms);
  EXPECT_EQ(default_ulpfec_codec_.id, config.rtp.fec.ulpfec_payload_type);
  EXPECT_EQ(default_red_codec_.id, config.rtp.fec.red_payload_type);

  EXPECT_EQ(1u, config.rtp.rtx.ssrcs.size());
  EXPECT_EQ(kRtxSsrcs1[0], config.rtp.rtx.ssrcs[0]);
  EXPECT_EQ(static_cast<int>(default_rtx_codec_.id),
            config.rtp.rtx.payload_type);
  // TODO(juberti): Check RTCP, PLI, TMMBR.
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsWithoutFec) {
  std::vector<VideoCodec> codecs;
  codecs.push_back(kVp8Codec);
  ASSERT_TRUE(channel_->SetSendCodecs(codecs));

  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Config config = stream->GetConfig();

  EXPECT_EQ(-1, config.rtp.fec.ulpfec_payload_type);
  EXPECT_EQ(-1, config.rtp.fec.red_payload_type);
}

TEST_F(WebRtcVideoChannel2Test,
       SetSendCodecRejectsRtxWithoutAssociatedPayloadType) {
  std::vector<VideoCodec> codecs;
  cricket::VideoCodec rtx_codec(96, "rtx", 0, 0, 0, 0);
  codecs.push_back(rtx_codec);
  EXPECT_FALSE(channel_->SetSendCodecs(codecs))
      << "RTX codec without associated payload type should be rejected.";
}

TEST_F(WebRtcVideoChannel2Test,
       SetSendCodecRejectsRtxWithoutMatchingVideoCodec) {
  std::vector<VideoCodec> codecs;
  cricket::VideoCodec rtx_codec =
      cricket::VideoCodec::CreateRtxCodec(96, kVp8Codec.id);
  codecs.push_back(kVp8Codec);
  codecs.push_back(rtx_codec);
  ASSERT_TRUE(channel_->SetSendCodecs(codecs));

  cricket::VideoCodec rtx_codec2 =
      cricket::VideoCodec::CreateRtxCodec(96, kVp8Codec.id + 1);
  codecs.pop_back();
  codecs.push_back(rtx_codec2);
  EXPECT_FALSE(channel_->SetSendCodecs(codecs))
      << "RTX without matching video codec should be rejected.";
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsWithoutFecDisablesFec) {
  std::vector<VideoCodec> codecs;
  codecs.push_back(kVp8Codec);
  codecs.push_back(kUlpfecCodec);
  ASSERT_TRUE(channel_->SetSendCodecs(codecs));

  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Config config = stream->GetConfig();

  EXPECT_EQ(kUlpfecCodec.id, config.rtp.fec.ulpfec_payload_type);

  codecs.pop_back();
  ASSERT_TRUE(channel_->SetSendCodecs(codecs));
  stream = fake_channel_->GetFakeCall()->GetVideoSendStreams()[0];
  ASSERT_TRUE(stream != NULL);
  config = stream->GetConfig();
  EXPECT_EQ(-1, config.rtp.fec.ulpfec_payload_type)
      << "SetSendCodec without FEC should disable current FEC.";
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsChangesExistingStreams) {
  std::vector<VideoCodec> codecs;
  codecs.push_back(kVp8Codec720p);
  ASSERT_TRUE(channel_->SetSendCodecs(codecs));

  std::vector<webrtc::VideoStream> streams =
      AddSendStream()->GetVideoStreams();
  EXPECT_EQ(kVp8Codec720p.width, streams[0].width);
  EXPECT_EQ(kVp8Codec720p.height, streams[0].height);

  codecs.clear();
  codecs.push_back(kVp8Codec360p);
  ASSERT_TRUE(channel_->SetSendCodecs(codecs));
  streams = fake_channel_->GetFakeCall()->GetVideoSendStreams()[0]
      ->GetVideoStreams();
  EXPECT_EQ(kVp8Codec360p.width, streams[0].width);
  EXPECT_EQ(kVp8Codec360p.height, streams[0].height);
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsWithMinMaxBitrate) {
  SetSendCodecsShouldWorkForBitrates("10", "20");
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsRejectsMaxLessThanMinBitrate) {
  std::vector<VideoCodec> video_codecs = engine_.codecs();
  video_codecs[0].params[kCodecParamMinBitrate] = "30";
  video_codecs[0].params[kCodecParamMaxBitrate] = "20";
  EXPECT_FALSE(channel_->SetSendCodecs(video_codecs));
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsAcceptLargeMinMaxBitrate) {
  SetSendCodecsShouldWorkForBitrates("1000", "2000");
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsWithMaxQuantization) {
  static const char* kMaxQuantization = "21";
  std::vector<VideoCodec> codecs;
  codecs.push_back(kVp8Codec);
  codecs[0].params[kCodecParamMaxQuantization] = kMaxQuantization;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_EQ(static_cast<unsigned int>(atoi(kMaxQuantization)),
            AddSendStream()->GetVideoStreams().back().max_qp);

  VideoCodec codec;
  EXPECT_TRUE(channel_->GetSendCodec(&codec));
  EXPECT_EQ(kMaxQuantization, codec.params[kCodecParamMaxQuantization]);
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsRejectBadDimensions) {
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVp8Codec);

  codecs[0].width = 0;
  EXPECT_FALSE(channel_->SetSendCodecs(codecs))
      << "Codec set though codec width is zero.";

  codecs[0].width = kVp8Codec.width;
  codecs[0].height = 0;
  EXPECT_FALSE(channel_->SetSendCodecs(codecs))
      << "Codec set though codec height is zero.";
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsRejectBadPayloadTypes) {
  // TODO(pbos): Should we only allow the dynamic range?
  static const size_t kNumIncorrectPayloads = 4;
  static const int kIncorrectPayloads[kNumIncorrectPayloads] = {-2, -1, 128,
                                                                129};
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVp8Codec);
  for (size_t i = 0; i < kNumIncorrectPayloads; ++i) {
    int payload_type = kIncorrectPayloads[i];
    codecs[0].id = payload_type;
    EXPECT_FALSE(channel_->SetSendCodecs(codecs))
        << "Bad payload type '" << payload_type << "' accepted.";
  }
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsAcceptAllValidPayloadTypes) {
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVp8Codec);
  for (int payload_type = 0; payload_type <= 127; ++payload_type) {
    codecs[0].id = payload_type;
    EXPECT_TRUE(channel_->SetSendCodecs(codecs))
        << "Payload type '" << payload_type << "' rejected.";
  }
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_ResetVieSendCodecOnNewFrameSize) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, SetRecvCodecsWithOnlyVp8) {
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVp8Codec);
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));
}

// Test that we set our inbound RTX codecs properly.
TEST_F(WebRtcVideoChannel2Test, SetRecvCodecsWithRtx) {
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVp8Codec);
  cricket::VideoCodec rtx_codec(96, "rtx", 0, 0, 0, 0);
  codecs.push_back(rtx_codec);
  EXPECT_FALSE(channel_->SetRecvCodecs(codecs))
      << "RTX codec without associated payload should be rejected.";

  codecs[1].SetParam("apt", kVp8Codec.id + 1);
  EXPECT_FALSE(channel_->SetRecvCodecs(codecs))
      << "RTX codec with invalid associated payload type should be rejected.";

  codecs[1].SetParam("apt", kVp8Codec.id);
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));

  cricket::VideoCodec rtx_codec2(97, "rtx", 0, 0, 0, 0);
  rtx_codec2.SetParam("apt", rtx_codec.id);
  codecs.push_back(rtx_codec2);

  EXPECT_FALSE(channel_->SetRecvCodecs(codecs)) << "RTX codec with another RTX "
                                                   "as associated payload type "
                                                   "should be rejected.";
}

TEST_F(WebRtcVideoChannel2Test, SetRecvCodecsDifferentPayloadType) {
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVp8Codec);
  codecs[0].id = 99;
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));
}

TEST_F(WebRtcVideoChannel2Test, SetRecvCodecsAcceptDefaultCodecs) {
  EXPECT_TRUE(channel_->SetRecvCodecs(engine_.codecs()));

  FakeVideoReceiveStream* stream = AddRecvStream();
  webrtc::VideoReceiveStream::Config config = stream->GetConfig();
  EXPECT_STREQ(engine_.codecs()[0].name.c_str(), config.codecs[0].plName);
  EXPECT_EQ(engine_.codecs()[0].id, config.codecs[0].plType);
}

TEST_F(WebRtcVideoChannel2Test, SetRecvCodecsRejectUnsupportedCodec) {
  std::vector<VideoCodec> codecs;
  codecs.push_back(kVp8Codec);
  codecs.push_back(VideoCodec(101, "WTF3", 640, 400, 30, 0));
  EXPECT_FALSE(channel_->SetRecvCodecs(codecs));
}

// TODO(pbos): Enable VP9 through external codec support
TEST_F(WebRtcVideoChannel2Test,
       DISABLED_SetRecvCodecsAcceptsMultipleVideoCodecs) {
  std::vector<VideoCodec> codecs;
  codecs.push_back(kVp8Codec);
  codecs.push_back(kVp9Codec);
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));
}

TEST_F(WebRtcVideoChannel2Test,
       DISABLED_SetRecvCodecsSetsFecForAllVideoCodecs) {
  std::vector<VideoCodec> codecs;
  codecs.push_back(kVp8Codec);
  codecs.push_back(kVp9Codec);
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));
  FAIL();  // TODO(pbos): Verify that the FEC parameters are set for all codecs.
}

TEST_F(WebRtcVideoChannel2Test, SetRecvCodecsWithoutFecDisablesFec) {
  std::vector<VideoCodec> codecs;
  codecs.push_back(kVp8Codec);
  codecs.push_back(kUlpfecCodec);
  ASSERT_TRUE(channel_->SetSendCodecs(codecs));

  FakeVideoReceiveStream* stream = AddRecvStream();
  webrtc::VideoReceiveStream::Config config = stream->GetConfig();

  EXPECT_EQ(kUlpfecCodec.id, config.rtp.fec.ulpfec_payload_type);

  codecs.pop_back();
  ASSERT_TRUE(channel_->SetRecvCodecs(codecs));
  stream = fake_channel_->GetFakeCall()->GetVideoReceiveStreams()[0];
  ASSERT_TRUE(stream != NULL);
  config = stream->GetConfig();
  EXPECT_EQ(-1, config.rtp.fec.ulpfec_payload_type)
      << "SetSendCodec without FEC should disable current FEC.";
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsRejectDuplicateFecPayloads) {
  std::vector<VideoCodec> codecs;
  codecs.push_back(kVp8Codec);
  codecs.push_back(kRedCodec);
  codecs[1].id = codecs[0].id;
  EXPECT_FALSE(channel_->SetRecvCodecs(codecs));
}

TEST_F(WebRtcVideoChannel2Test, SetRecvCodecsRejectDuplicateCodecPayloads) {
  std::vector<VideoCodec> codecs;
  codecs.push_back(kVp8Codec);
  codecs.push_back(kVp9Codec);
  codecs[1].id = codecs[0].id;
  EXPECT_FALSE(channel_->SetRecvCodecs(codecs));
}

TEST_F(WebRtcVideoChannel2Test,
       SetRecvCodecsAcceptSameCodecOnMultiplePayloadTypes) {
  std::vector<VideoCodec> codecs;
  codecs.push_back(kVp8Codec);
  codecs.push_back(kVp8Codec);
  codecs[1].id += 1;
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));
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

TEST_F(WebRtcVideoChannel2Test, DISABLED_TestSetDscpOptions) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_SetOptionsWithMaxBitrate) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_SetOptionsWithLoweredBitrate) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_SetOptionsSucceedsWhenSending) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_ResetCodecOnScreencast) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_DontResetCodecOnSendFrame) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test,
       DISABLED_DontRegisterDecoderIfFactoryIsNotGiven) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_RegisterDecoderIfFactoryIsGiven) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_DontRegisterDecoderMultipleTimes) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_DontRegisterDecoderForNonVP8) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test,
       DISABLED_DontRegisterEncoderIfFactoryIsNotGiven) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_RegisterEncoderIfFactoryIsGiven) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_DontRegisterEncoderMultipleTimes) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test,
       DISABLED_RegisterEncoderWithMultipleSendStreams) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_DontRegisterEncoderForNonVP8) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_FeedbackParamsForNonVP8) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_ExternalCodecAddedToTheEnd) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_ExternalCodecIgnored) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_UpdateEncoderCodecsAfterSetFactory) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_OnReadyToSend) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_CaptureFrameTimestampToNtpTimestamp) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}
}  // namespace cricket
