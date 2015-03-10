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
#include "talk/media/webrtc/fakewebrtcvideoengine.h"
#include "talk/media/webrtc/simulcast.h"
#include "talk/media/webrtc/webrtcvideochannelfactory.h"
#include "talk/media/webrtc/webrtcvideoengine2.h"
#include "talk/media/webrtc/webrtcvideoengine2_unittest.h"
#include "talk/media/webrtc/webrtcvoiceengine.h"
#include "webrtc/base/arraysize.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/stringutils.h"
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

static const uint32 kSsrcs1[] = {1};
static const uint32 kSsrcs3[] = {1, 2, 3};
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

static void CreateBlackFrame(webrtc::I420VideoFrame* video_frame,
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

}  // namespace

namespace cricket {
FakeVideoSendStream::FakeVideoSendStream(
    const webrtc::VideoSendStream::Config& config,
    const webrtc::VideoEncoderConfig& encoder_config)
    : sending_(false),
      config_(config),
      codec_settings_set_(false),
      num_swapped_frames_(0) {
  assert(config.encoder_settings.encoder != NULL);
  ReconfigureVideoEncoder(encoder_config);
}

webrtc::VideoSendStream::Config FakeVideoSendStream::GetConfig() const {
  return config_;
}

webrtc::VideoEncoderConfig FakeVideoSendStream::GetEncoderConfig() const {
  return encoder_config_;
}

std::vector<webrtc::VideoStream> FakeVideoSendStream::GetVideoStreams() {
  return encoder_config_.streams;
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

int FakeVideoSendStream::GetNumberOfSwappedFrames() const {
  return num_swapped_frames_;
}

int FakeVideoSendStream::GetLastWidth() const {
  return last_frame_.width();
}

int FakeVideoSendStream::GetLastHeight() const {
  return last_frame_.height();
}

void FakeVideoSendStream::SwapFrame(webrtc::I420VideoFrame* frame) {
  ++num_swapped_frames_;
  last_frame_.SwapFrame(frame);
}

void FakeVideoSendStream::SetStats(
    const webrtc::VideoSendStream::Stats& stats) {
  stats_ = stats;
}

webrtc::VideoSendStream::Stats FakeVideoSendStream::GetStats() {
  return stats_;
}

bool FakeVideoSendStream::ReconfigureVideoEncoder(
    const webrtc::VideoEncoderConfig& config) {
  encoder_config_ = config;
  if (config.encoder_specific_settings != NULL) {
    assert(config_.encoder_settings.payload_name == "VP8");
    vp8_settings_ = *reinterpret_cast<const webrtc::VideoCodecVP8*>(
                        config.encoder_specific_settings);
  }
  codec_settings_set_ = config.encoder_specific_settings != NULL;
  return true;
}

webrtc::VideoSendStreamInput* FakeVideoSendStream::Input() {
  return this;
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

void FakeVideoReceiveStream::InjectFrame(const webrtc::I420VideoFrame& frame,
                                         int time_to_render_ms) {
  config_.renderer->RenderFrame(frame, time_to_render_ms);
}

webrtc::VideoReceiveStream::Stats FakeVideoReceiveStream::GetStats() const {
  return stats_;
}

void FakeVideoReceiveStream::Start() {
  receiving_ = true;
}

void FakeVideoReceiveStream::Stop() {
  receiving_ = false;
}

void FakeVideoReceiveStream::SetStats(
    const webrtc::VideoReceiveStream::Stats& stats) {
  stats_ = stats;
}

FakeCall::FakeCall(const webrtc::Call::Config& config)
    : config_(config),
      network_state_(kNetworkUp),
      num_created_send_streams_(0),
      num_created_receive_streams_(0) {
  SetVideoCodecs(GetDefaultVideoCodecs());
}

FakeCall::~FakeCall() {
  EXPECT_EQ(0u, video_send_streams_.size());
  EXPECT_EQ(0u, video_receive_streams_.size());
}

void FakeCall::SetVideoCodecs(const std::vector<webrtc::VideoCodec> codecs) {
  codecs_ = codecs;
}

webrtc::Call::Config FakeCall::GetConfig() const {
  return config_;
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
  rtc::strcpyn(
      vp8_codec.plName, ARRAY_SIZE(vp8_codec.plName), kVp8Codec.name.c_str());
  vp8_codec.plType = kVp8Codec.id;

  return vp8_codec;
}

webrtc::VideoCodec FakeCall::GetVideoCodecVp9() {
  webrtc::VideoCodec vp9_codec = GetEmptyVideoCodec();
  // TODO(pbos): Add a correct codecType when webrtc has one.
  vp9_codec.codecType = webrtc::kVideoCodecVP8;
  rtc::strcpyn(
      vp9_codec.plName, ARRAY_SIZE(vp9_codec.plName), kVp9Codec.name.c_str());
  vp9_codec.plType = kVp9Codec.id;

  return vp9_codec;
}

std::vector<webrtc::VideoCodec> FakeCall::GetDefaultVideoCodecs() {
  std::vector<webrtc::VideoCodec> codecs;
  codecs.push_back(GetVideoCodecVp8());
  //    codecs.push_back(GetVideoCodecVp9());

  return codecs;
}

webrtc::Call::NetworkState FakeCall::GetNetworkState() const {
  return network_state_;
}

webrtc::VideoSendStream* FakeCall::CreateVideoSendStream(
    const webrtc::VideoSendStream::Config& config,
    const webrtc::VideoEncoderConfig& encoder_config) {
  FakeVideoSendStream* fake_stream =
      new FakeVideoSendStream(config, encoder_config);
  video_send_streams_.push_back(fake_stream);
  ++num_created_send_streams_;
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
  ++num_created_receive_streams_;
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
  return this;
}

FakeCall::DeliveryStatus FakeCall::DeliverPacket(const uint8_t* packet,
                                                 size_t length) {
  CHECK(length >= 12);
  uint32_t ssrc;
  if (!GetRtpSsrc(packet, length, &ssrc))
    return DELIVERY_PACKET_ERROR;

  for (auto& receiver: video_receive_streams_) {
    if (receiver->GetConfig().rtp.remote_ssrc == ssrc)
        return DELIVERY_OK;
  }
  return DELIVERY_UNKNOWN_SSRC;
}

void FakeCall::SetStats(const webrtc::Call::Stats& stats) {
  stats_ = stats;
}

int FakeCall::GetNumCreatedSendStreams() const {
  return num_created_send_streams_;
}

int FakeCall::GetNumCreatedReceiveStreams() const {
  return num_created_receive_streams_;
}

webrtc::Call::Stats FakeCall::GetStats() const {
  return stats_;
}

void FakeCall::SetBitrateConfig(
    const webrtc::Call::Config::BitrateConfig& bitrate_config) {
  config_.stream_bitrates = bitrate_config;
}

void FakeCall::SignalNetworkState(webrtc::Call::NetworkState state) {
  network_state_ = state;
}

class WebRtcVideoEngine2Test : public ::testing::Test {
 public:
  WebRtcVideoEngine2Test() : WebRtcVideoEngine2Test(nullptr) {}
  WebRtcVideoEngine2Test(WebRtcVoiceEngine* voice_engine)
      : engine_(voice_engine) {
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
  class FakeCallFactory : public WebRtcCallFactory {
   public:
    FakeCallFactory() : fake_call_(NULL) {}
    FakeCall* GetCall() { return fake_call_; }

   private:
    webrtc::Call* CreateCall(const webrtc::Call::Config& config) override {
      assert(fake_call_ == NULL);
      fake_call_ = new FakeCall(config);
      return fake_call_;
    }

    FakeCall* fake_call_;
  };

  VideoMediaChannel* SetUpForExternalEncoderFactory(
      cricket::WebRtcVideoEncoderFactory* encoder_factory,
      const std::vector<VideoCodec>& codecs);

  VideoMediaChannel* SetUpForExternalDecoderFactory(
      cricket::WebRtcVideoDecoderFactory* decoder_factory,
      const std::vector<VideoCodec>& codecs);

  // Used in WebRtcVideoEngine2VoiceTest, but defined here so it's properly
  // initialized when the constructor is called.
  WebRtcVoiceEngine voice_engine_;
  WebRtcVideoEngine2 engine_;
  VideoCodec default_codec_;
  VideoCodec default_red_codec_;
  VideoCodec default_ulpfec_codec_;
  VideoCodec default_rtx_codec_;
};

class WebRtcVideoEngine2VoiceTest : public WebRtcVideoEngine2Test {
 public:
  WebRtcVideoEngine2VoiceTest() : WebRtcVideoEngine2Test(&voice_engine_) {}
};

TEST_F(WebRtcVideoEngine2VoiceTest, ConfiguresAvSyncForFirstReceiveChannel) {
  FakeCallFactory call_factory;
  engine_.SetCallFactory(&call_factory);

  voice_engine_.Init(rtc::Thread::Current());
  engine_.Init(rtc::Thread::Current());

  rtc::scoped_ptr<VoiceMediaChannel> voice_channel(
      voice_engine_.CreateChannel());
  ASSERT_TRUE(voice_channel.get() != NULL);
  WebRtcVoiceMediaChannel* webrtc_voice_channel =
      static_cast<WebRtcVoiceMediaChannel*>(voice_channel.get());
  ASSERT_NE(webrtc_voice_channel->voe_channel(), -1);
  rtc::scoped_ptr<VideoMediaChannel> channel(
      engine_.CreateChannel(cricket::VideoOptions(), voice_channel.get()));

  FakeCall* fake_call = call_factory.GetCall();
  ASSERT_TRUE(fake_call != NULL);

  webrtc::Call::Config call_config = fake_call->GetConfig();

  ASSERT_TRUE(voice_engine_.voe()->engine() != NULL);
  ASSERT_EQ(voice_engine_.voe()->engine(), call_config.voice_engine);

  EXPECT_TRUE(channel->AddRecvStream(StreamParams::CreateLegacy(kSsrc)));
  EXPECT_TRUE(channel->AddRecvStream(StreamParams::CreateLegacy(kSsrc + 1)));
  std::vector<FakeVideoReceiveStream*> receive_streams =
      fake_call->GetVideoReceiveStreams();

  ASSERT_EQ(2u, receive_streams.size());
  EXPECT_EQ(webrtc_voice_channel->voe_channel(),
            receive_streams[0]->GetConfig().audio_channel_id);
  EXPECT_EQ(-1, receive_streams[1]->GetConfig().audio_channel_id)
      << "AV sync should only be set up for the first receive channel.";
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

TEST_F(WebRtcVideoEngine2Test, SetDefaultEncoderConfigPreservesFeedbackParams) {
  cricket::VideoCodec max_settings(
      engine_.codecs()[0].id, engine_.codecs()[0].name,
      engine_.codecs()[0].width / 2, engine_.codecs()[0].height / 2, 30, 0);
  // This codec shouldn't have NACK by default or the test is pointless.
  EXPECT_FALSE(max_settings.HasFeedbackParam(
      FeedbackParam(kRtcpFbParamNack, kParamValueEmpty)));
  // The engine should by default have it however.
  EXPECT_TRUE(engine_.codecs()[0].HasFeedbackParam(
      FeedbackParam(kRtcpFbParamNack, kParamValueEmpty)));

  // Set constrained max codec settings.
  EXPECT_TRUE(engine_.SetDefaultEncoderConfig(
      cricket::VideoEncoderConfig(max_settings)));

  // Verify that feedback parameters are retained.
  EXPECT_TRUE(engine_.codecs()[0].HasFeedbackParam(
      FeedbackParam(kRtcpFbParamNack, kParamValueEmpty)));
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
  engine_.Init(rtc::Thread::Current());
  rtc::scoped_ptr<VideoMediaChannel> channel(
      engine_.CreateChannel(cricket::VideoOptions(), NULL));

  EXPECT_TRUE(channel->AddSendStream(StreamParams::CreateLegacy(123)));

  EXPECT_FALSE(channel->SetSend(true))
      << "Channel should not start without codecs.";
  EXPECT_TRUE(channel->SetSend(false))
      << "Channel should be stoppable even without set codecs.";
}

TEST_F(WebRtcVideoEngine2Test, GetStatsWithoutSendCodecsSetDoesNotCrash) {
  engine_.Init(rtc::Thread::Current());
  rtc::scoped_ptr<VideoMediaChannel> channel(
      engine_.CreateChannel(cricket::VideoOptions(), NULL));
  EXPECT_TRUE(channel->AddSendStream(StreamParams::CreateLegacy(123)));
  VideoMediaInfo info;
  channel->GetStats(&info);
}

TEST_F(WebRtcVideoEngine2Test, UseExternalFactoryForVp8WhenSupported) {
  cricket::FakeWebRtcVideoEncoderFactory encoder_factory;
  encoder_factory.AddSupportedVideoCodecType(webrtc::kVideoCodecVP8, "VP8");
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVp8Codec);

  rtc::scoped_ptr<VideoMediaChannel> channel(
      SetUpForExternalEncoderFactory(&encoder_factory, codecs));

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
  EXPECT_TRUE(channel->SetSendCodecs(codecs));
  EXPECT_EQ(num_created_encoders, encoder_factory.GetNumCreatedEncoders());

  // Remove stream previously added to free the external encoder instance.
  EXPECT_TRUE(channel->RemoveSendStream(kSsrc));
  EXPECT_EQ(0u, encoder_factory.encoders().size());
}

VideoMediaChannel* WebRtcVideoEngine2Test::SetUpForExternalEncoderFactory(
    cricket::WebRtcVideoEncoderFactory* encoder_factory,
    const std::vector<VideoCodec>& codecs) {
  engine_.SetExternalEncoderFactory(encoder_factory);
  engine_.Init(rtc::Thread::Current());

  VideoMediaChannel* channel =
      engine_.CreateChannel(cricket::VideoOptions(), NULL);
  EXPECT_TRUE(channel->SetSendCodecs(codecs));

  return channel;
}

VideoMediaChannel* WebRtcVideoEngine2Test::SetUpForExternalDecoderFactory(
    cricket::WebRtcVideoDecoderFactory* decoder_factory,
    const std::vector<VideoCodec>& codecs) {
  engine_.SetExternalDecoderFactory(decoder_factory);
  engine_.Init(rtc::Thread::Current());

  VideoMediaChannel* channel =
      engine_.CreateChannel(cricket::VideoOptions(), NULL);
  EXPECT_TRUE(channel->SetRecvCodecs(codecs));

  return channel;
}

TEST_F(WebRtcVideoEngine2Test, UsesSimulcastAdapterForVp8Factories) {
  cricket::FakeWebRtcVideoEncoderFactory encoder_factory;
  encoder_factory.AddSupportedVideoCodecType(webrtc::kVideoCodecVP8, "VP8");
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVp8Codec);

  rtc::scoped_ptr<VideoMediaChannel> channel(
      SetUpForExternalEncoderFactory(&encoder_factory, codecs));

  std::vector<uint32> ssrcs = MAKE_VECTOR(kSsrcs3);

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

  codecs.clear();
  codecs.push_back(kVp8Codec);
  EXPECT_TRUE(channel->SetSendCodecs(codecs));

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

  std::vector<uint32> ssrcs = MAKE_VECTOR(kSsrcs3);

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

// Test external codec with be added to the end of the supported codec list.
TEST_F(WebRtcVideoEngine2Test, ReportSupportedExternalCodecs) {
  cricket::FakeWebRtcVideoEncoderFactory encoder_factory;
  encoder_factory.AddSupportedVideoCodecType(webrtc::kVideoCodecH264, "H264");
  engine_.SetExternalEncoderFactory(&encoder_factory);

  engine_.Init(rtc::Thread::Current());

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
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVp8Codec);

  rtc::scoped_ptr<VideoMediaChannel> channel(
      SetUpForExternalDecoderFactory(&decoder_factory, codecs));

  EXPECT_TRUE(
      channel->AddRecvStream(cricket::StreamParams::CreateLegacy(kSsrc)));
  ASSERT_EQ(1u, decoder_factory.decoders().size());

  // Setting codecs of the same type should not reallocate the decoder.
  EXPECT_TRUE(channel->SetRecvCodecs(codecs));
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
  typedef VideoMediaChannelTest<WebRtcVideoEngine2, WebRtcVideoChannel2> Base;

  cricket::VideoCodec DefaultCodec() override { return kVp8Codec; }
};

#define WEBRTC_BASE_TEST(test) \
  TEST_F(WebRtcVideoChannel2BaseTest, test) { Base::test(); }

#define WEBRTC_DISABLED_BASE_TEST(test) \
  TEST_F(WebRtcVideoChannel2BaseTest, DISABLED_##test) { Base::test(); }

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
  cricket::VideoCodec codec = kVp8Codec720p;
  EXPECT_TRUE(SetOneCodec(codec));
  codec.width /= 2;
  codec.height /= 2;
  EXPECT_TRUE(SetSend(true));
  EXPECT_TRUE(channel_->SetRender(true));
  EXPECT_EQ(0, renderer_.num_rendered_frames());
  EXPECT_TRUE(SendFrame());
  EXPECT_FRAME_WAIT(1, codec.width, codec.height, kTimeout);
}

class WebRtcVideoChannel2Test : public WebRtcVideoEngine2Test,
                                public WebRtcCallFactory {
 public:
  WebRtcVideoChannel2Test() : fake_call_(NULL) {}
  void SetUp() override {
    engine_.SetCallFactory(this);
    engine_.Init(rtc::Thread::Current());
    channel_.reset(engine_.CreateChannel(cricket::VideoOptions(), NULL));
    ASSERT_TRUE(fake_call_ != NULL) << "Call not created through factory.";
    last_ssrc_ = 123;
    ASSERT_TRUE(channel_->SetSendCodecs(engine_.codecs()));
  }

 protected:
  webrtc::Call* CreateCall(const webrtc::Call::Config& config) override {
    assert(fake_call_ == NULL);
    fake_call_ = new FakeCall(config);
    return fake_call_;
  }

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
    std::vector<VideoCodec> codecs;
    codecs.push_back(kVp8Codec);
    codecs[0].params[kCodecParamMinBitrate] = min_bitrate_kbps;
    codecs[0].params[kCodecParamStartBitrate] = start_bitrate_kbps;
    codecs[0].params[kCodecParamMaxBitrate] = max_bitrate_kbps;
    EXPECT_TRUE(channel_->SetSendCodecs(codecs));

    EXPECT_EQ(expected_min_bitrate_bps,
              fake_call_->GetConfig().stream_bitrates.min_bitrate_bps);
    EXPECT_EQ(expected_start_bitrate_bps,
              fake_call_->GetConfig().stream_bitrates.start_bitrate_bps);
    EXPECT_EQ(expected_max_bitrate_bps,
              fake_call_->GetConfig().stream_bitrates.max_bitrate_bps);
  }

  void TestSetSendRtpHeaderExtensions(const std::string& cricket_ext,
                                      const std::string& webrtc_ext) {
    FakeCall* call = fake_call_;
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
    FakeCall* call = fake_call_;
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

  void TestCpuAdaptation(bool enable_overuse);

  FakeCall* fake_call_;
  rtc::scoped_ptr<VideoMediaChannel> channel_;
  uint32 last_ssrc_;
};

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

TEST_F(WebRtcVideoChannel2Test, IdenticalSendExtensionsDoesntRecreateStream) {
  const int kTOffsetId = 1;
  const int kAbsSendTimeId = 2;
  std::vector<cricket::RtpHeaderExtension> extensions;
  extensions.push_back(cricket::RtpHeaderExtension(
      kRtpAbsoluteSenderTimeHeaderExtension, kAbsSendTimeId));
  extensions.push_back(cricket::RtpHeaderExtension(
      kRtpTimestampOffsetHeaderExtension, kTOffsetId));

  EXPECT_TRUE(channel_->SetSendRtpHeaderExtensions(extensions));
  FakeVideoSendStream* send_stream =
      AddSendStream(cricket::StreamParams::CreateLegacy(123));

  EXPECT_EQ(1, fake_call_->GetNumCreatedSendStreams());
  ASSERT_EQ(2u, send_stream->GetConfig().rtp.extensions.size());

  // Setting the same extensions (even if in different order) shouldn't
  // reallocate the stream.
  std::reverse(extensions.begin(), extensions.end());
  EXPECT_TRUE(channel_->SetSendRtpHeaderExtensions(extensions));

  EXPECT_EQ(1, fake_call_->GetNumCreatedSendStreams());

  // Setting different extensions should recreate the stream.
  extensions.resize(1);
  EXPECT_TRUE(channel_->SetSendRtpHeaderExtensions(extensions));

  EXPECT_EQ(2, fake_call_->GetNumCreatedSendStreams());
}

TEST_F(WebRtcVideoChannel2Test, IdenticalRecvExtensionsDoesntRecreateStream) {
  const int kTOffsetId = 1;
  const int kAbsSendTimeId = 2;
  std::vector<cricket::RtpHeaderExtension> extensions;
  extensions.push_back(cricket::RtpHeaderExtension(
      kRtpAbsoluteSenderTimeHeaderExtension, kAbsSendTimeId));
  extensions.push_back(cricket::RtpHeaderExtension(
      kRtpTimestampOffsetHeaderExtension, kTOffsetId));

  EXPECT_TRUE(channel_->SetRecvRtpHeaderExtensions(extensions));
  FakeVideoReceiveStream* send_stream =
      AddRecvStream(cricket::StreamParams::CreateLegacy(123));

  EXPECT_EQ(1, fake_call_->GetNumCreatedReceiveStreams());
  ASSERT_EQ(2u, send_stream->GetConfig().rtp.extensions.size());

  // Setting the same extensions (even if in different order) shouldn't
  // reallocate the stream.
  std::reverse(extensions.begin(), extensions.end());
  EXPECT_TRUE(channel_->SetRecvRtpHeaderExtensions(extensions));

  EXPECT_EQ(1, fake_call_->GetNumCreatedReceiveStreams());

  // Setting different extensions should recreate the stream.
  extensions.resize(1);
  EXPECT_TRUE(channel_->SetRecvRtpHeaderExtensions(extensions));

  EXPECT_EQ(2, fake_call_->GetNumCreatedReceiveStreams());
}

TEST_F(WebRtcVideoChannel2Test,
       SetSendRtpHeaderExtensionsExcludeUnsupportedExtensions) {
  const int kUnsupportedId = 1;
  const int kTOffsetId = 2;

  std::vector<cricket::RtpHeaderExtension> extensions;
  extensions.push_back(
      cricket::RtpHeaderExtension(kUnsupportedExtensionName, kUnsupportedId));
  extensions.push_back(
      cricket::RtpHeaderExtension(webrtc::RtpExtension::kTOffset, kTOffsetId));
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
  extensions.push_back(
      cricket::RtpHeaderExtension(kUnsupportedExtensionName, kUnsupportedId));
  extensions.push_back(
      cricket::RtpHeaderExtension(webrtc::RtpExtension::kTOffset, kTOffsetId));
  EXPECT_TRUE(channel_->SetRecvRtpHeaderExtensions(extensions));
  FakeVideoReceiveStream* recv_stream =
      AddRecvStream(cricket::StreamParams::CreateLegacy(123));

  // Only timestamp offset extension is set to receive stream,
  // unsupported rtp extension is ignored.
  ASSERT_EQ(1u, recv_stream->GetConfig().rtp.extensions.size());
  EXPECT_STREQ(webrtc::RtpExtension::kTOffset,
               recv_stream->GetConfig().rtp.extensions[0].name.c_str());
}

TEST_F(WebRtcVideoChannel2Test, SetSendRtpHeaderExtensionsRejectsIncorrectIds) {
  const int kIncorrectIds[] = {-2, -1, 15, 16};
  for (size_t i = 0; i < arraysize(kIncorrectIds); ++i) {
    std::vector<cricket::RtpHeaderExtension> extensions;
    extensions.push_back(cricket::RtpHeaderExtension(
        webrtc::RtpExtension::kTOffset, kIncorrectIds[i]));
    EXPECT_FALSE(channel_->SetSendRtpHeaderExtensions(extensions))
        << "Bad extension id '" << kIncorrectIds[i] << "' accepted.";
  }
}

TEST_F(WebRtcVideoChannel2Test, SetRecvRtpHeaderExtensionsRejectsIncorrectIds) {
  const int kIncorrectIds[] = {-2, -1, 15, 16};
  for (size_t i = 0; i < arraysize(kIncorrectIds); ++i) {
    std::vector<cricket::RtpHeaderExtension> extensions;
    extensions.push_back(cricket::RtpHeaderExtension(
        webrtc::RtpExtension::kTOffset, kIncorrectIds[i]));
    EXPECT_FALSE(channel_->SetRecvRtpHeaderExtensions(extensions))
        << "Bad extension id '" << kIncorrectIds[i] << "' accepted.";
  }
}

TEST_F(WebRtcVideoChannel2Test, SetSendRtpHeaderExtensionsRejectsDuplicateIds) {
  const int id = 1;
  std::vector<cricket::RtpHeaderExtension> extensions;
  extensions.push_back(
      cricket::RtpHeaderExtension(webrtc::RtpExtension::kTOffset, id));
  extensions.push_back(
      cricket::RtpHeaderExtension(kRtpAbsoluteSenderTimeHeaderExtension, id));
  EXPECT_FALSE(channel_->SetSendRtpHeaderExtensions(extensions));

  // Duplicate entries are also not supported.
  extensions.clear();
  extensions.push_back(
      cricket::RtpHeaderExtension(webrtc::RtpExtension::kTOffset, id));
  extensions.push_back(extensions.back());
  EXPECT_FALSE(channel_->SetSendRtpHeaderExtensions(extensions));
}

TEST_F(WebRtcVideoChannel2Test, SetRecvRtpHeaderExtensionsRejectsDuplicateIds) {
  const int id = 1;
  std::vector<cricket::RtpHeaderExtension> extensions;
  extensions.push_back(
      cricket::RtpHeaderExtension(webrtc::RtpExtension::kTOffset, id));
  extensions.push_back(
      cricket::RtpHeaderExtension(kRtpAbsoluteSenderTimeHeaderExtension, id));
  EXPECT_FALSE(channel_->SetRecvRtpHeaderExtensions(extensions));

  // Duplicate entries are also not supported.
  extensions.clear();
  extensions.push_back(
      cricket::RtpHeaderExtension(webrtc::RtpExtension::kTOffset, id));
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
  EXPECT_EQ(1u, fake_call_->GetVideoReceiveStreams().size());
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
  stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_FALSE(stream->GetConfig().rtp.remb);

  // Verify that REMB is turned on when setting default codecs since the
  // default codecs have REMB enabled.
  EXPECT_TRUE(channel_->SetRecvCodecs(engine_.codecs()));
  stream = fake_call_->GetVideoReceiveStreams()[0];
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

TEST_F(WebRtcVideoChannel2Test, UsesCorrectSettingsForScreencast) {
  static const int kScreenshareMinBitrateKbps = 800;
  cricket::VideoCodec codec = kVp8Codec360p;
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  VideoOptions options;
  options.screencast_min_bitrate.Set(kScreenshareMinBitrateKbps);
  channel_->SetOptions(options);

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
  EXPECT_EQ(webrtc::VideoEncoderConfig::kRealtimeVideo,
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
  EXPECT_EQ(webrtc::VideoEncoderConfig::kScreenshare,
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
  static const int kConferenceScreencastTemporalBitrateBps = 100000;
  VideoOptions options;
  options.conference_mode.Set(true);
  channel_->SetOptions(options);

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
  EXPECT_EQ(webrtc::VideoEncoderConfig::kScreenshare,
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
  VideoOptions options;
  options.suspend_below_min_bitrate.Set(true);
  channel_->SetOptions(options);

  FakeVideoSendStream* stream = AddSendStream();
  EXPECT_TRUE(stream->GetConfig().suspend_below_min_bitrate);

  options.suspend_below_min_bitrate.Set(false);
  channel_->SetOptions(options);

  stream = fake_call_->GetVideoSendStreams()[0];
  EXPECT_FALSE(stream->GetConfig().suspend_below_min_bitrate);
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

  stream = fake_call_->GetVideoSendStreams()[0];
  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  EXPECT_TRUE(vp8_settings.denoisingOn);
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_MultipleSendStreamsWithOneCapturer) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, DISABLED_SendReceiveBitratesStats) {
  FAIL() << "Not implemented.";  // TODO(pbos): Implement.
}

TEST_F(WebRtcVideoChannel2Test, AdaptsOnOveruse) {
  TestCpuAdaptation(true);
}

TEST_F(WebRtcVideoChannel2Test, DoesNotAdaptOnOveruseWhenDisabled) {
  TestCpuAdaptation(false);
}

void WebRtcVideoChannel2Test::TestCpuAdaptation(bool enable_overuse) {
  cricket::VideoCodec codec = kVp8Codec720p;
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  if (enable_overuse) {
    VideoOptions options;
    options.cpu_overuse_detection.Set(true);
    channel_->SetOptions(options);
  }

  AddSendStream();

  cricket::FakeVideoCapturer capturer;
  EXPECT_TRUE(channel_->SetCapturer(last_ssrc_, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));

  EXPECT_TRUE(channel_->SetSend(true));

  // Trigger overuse.
  webrtc::LoadObserver* overuse_callback =
      fake_call_->GetConfig().overuse_callback;
  ASSERT_TRUE(overuse_callback != NULL);
  overuse_callback->OnLoadUpdate(webrtc::LoadObserver::kOveruse);

  EXPECT_TRUE(capturer.CaptureFrame());
  ASSERT_EQ(1u, fake_call_->GetVideoSendStreams().size());
  FakeVideoSendStream* send_stream = fake_call_->GetVideoSendStreams().front();

  EXPECT_EQ(1, send_stream->GetNumberOfSwappedFrames());

  if (enable_overuse) {
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

TEST_F(WebRtcVideoChannel2Test, EstimatesNtpStartTimeAndElapsedTimeCorrectly) {
  // Start at last timestamp to verify that wraparounds are estimated correctly.
  static const uint32_t kInitialTimestamp = 0xFFFFFFFFu;
  static const int64_t kInitialNtpTimeMs = 1247891230;
  static const int kFrameOffsetMs = 20;
  EXPECT_TRUE(channel_->SetRecvCodecs(engine_.codecs()));

  FakeVideoReceiveStream* stream = AddRecvStream();
  cricket::FakeVideoRenderer renderer;
  EXPECT_TRUE(channel_->SetRenderer(last_ssrc_, &renderer));
  EXPECT_TRUE(channel_->SetRender(true));

  webrtc::I420VideoFrame video_frame;
  CreateBlackFrame(&video_frame, 4, 4);
  video_frame.set_timestamp(kInitialTimestamp);
  // Initial NTP time is not available on the first frame, but should still be
  // able to be estimated.
  stream->InjectFrame(video_frame, 0);

  EXPECT_EQ(1, renderer.num_rendered_frames());
  EXPECT_EQ(0, renderer.last_frame_elapsed_time_ns());

  // This timestamp is kInitialTimestamp (-1) + kFrameOffsetMs * 90, which
  // triggers a constant-overflow warning, hence we're calculating it explicitly
  // here.
  video_frame.set_timestamp(kFrameOffsetMs * 90 - 1);
  video_frame.set_ntp_time_ms(kInitialNtpTimeMs + kFrameOffsetMs);
  stream->InjectFrame(video_frame, 0);

  EXPECT_EQ(2, renderer.num_rendered_frames());
  EXPECT_EQ(kFrameOffsetMs * rtc::kNumNanosecsPerMillisec,
            renderer.last_frame_elapsed_time_ns());

  // Verify that NTP time has been correctly deduced.
  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1u, info.receivers.size());
  EXPECT_EQ(kInitialNtpTimeMs, info.receivers[0].capture_start_ntp_time_ms);
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
  EXPECT_EQ(default_rtx_codec_.id, config.rtp.rtx.payload_type);
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
  stream = fake_call_->GetVideoSendStreams()[0];
  ASSERT_TRUE(stream != NULL);
  config = stream->GetConfig();
  EXPECT_EQ(-1, config.rtp.fec.ulpfec_payload_type)
      << "SetSendCodec without FEC should disable current FEC.";
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsChangesExistingStreams) {
  std::vector<VideoCodec> codecs;
  codecs.push_back(kVp8Codec720p);
  ASSERT_TRUE(channel_->SetSendCodecs(codecs));
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

  codecs.clear();
  codecs.push_back(kVp8Codec360p);
  ASSERT_TRUE(channel_->SetSendCodecs(codecs));
  streams = fake_call_->GetVideoSendStreams()[0]->GetVideoStreams();
  EXPECT_EQ(kVp8Codec360p.width, streams[0].width);
  EXPECT_EQ(kVp8Codec360p.height, streams[0].height);
  EXPECT_TRUE(channel_->SetCapturer(last_ssrc_, NULL));
}

TEST_F(WebRtcVideoChannel2Test, SetSendCodecsWithBitrates) {
  SetSendCodecsShouldWorkForBitrates("100", 100000, "150", 150000, "200",
                                     200000);
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
  std::vector<VideoCodec> video_codecs = engine_.codecs();
  video_codecs[0].params[kCodecParamMinBitrate] = "300";
  video_codecs[0].params[kCodecParamMaxBitrate] = "200";
  EXPECT_FALSE(channel_->SetSendCodecs(video_codecs));
}

TEST_F(WebRtcVideoChannel2Test,
       SetMaxSendBandwidthShouldPreserveOtherBitrates) {
  SetSendCodecsShouldWorkForBitrates("100", 100000, "150", 150000, "200",
                                     200000);
  channel_->SetMaxSendBandwidth(300000);
  EXPECT_EQ(100000, fake_call_->GetConfig().stream_bitrates.min_bitrate_bps)
      << "Setting max bitrate should keep previous min bitrate.";
  EXPECT_EQ(-1, fake_call_->GetConfig().stream_bitrates.start_bitrate_bps)
      << "Setting max bitrate should not reset start bitrate.";
  EXPECT_EQ(300000, fake_call_->GetConfig().stream_bitrates.max_bitrate_bps);
}

TEST_F(WebRtcVideoChannel2Test, SetMaxSendBandwidthShouldBeRemovable) {
  channel_->SetMaxSendBandwidth(300000);
  EXPECT_EQ(300000, fake_call_->GetConfig().stream_bitrates.max_bitrate_bps);
  // <= 0 means disable (infinite) max bitrate.
  channel_->SetMaxSendBandwidth(0);
  EXPECT_EQ(-1, fake_call_->GetConfig().stream_bitrates.max_bitrate_bps)
      << "Setting zero max bitrate did not reset start bitrate.";
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
  static const int kIncorrectPayloads[] = {-2, -1, 128, 129};
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(kVp8Codec);
  for (size_t i = 0; i < arraysize(kIncorrectPayloads); ++i) {
    codecs[0].id = kIncorrectPayloads[i];
    EXPECT_FALSE(channel_->SetSendCodecs(codecs))
        << "Bad payload type '" << kIncorrectPayloads[i] << "' accepted.";
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
  EXPECT_EQ(engine_.codecs()[0].name, config.decoders[0].payload_name);
  EXPECT_EQ(engine_.codecs()[0].id, config.decoders[0].payload_type);
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
  stream = fake_call_->GetVideoReceiveStreams()[0];
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

// This test verifies DSCP settings are properly applied on video media channel.
TEST_F(WebRtcVideoChannel2Test, TestSetDscpOptions) {
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

TEST_F(WebRtcVideoChannel2Test, OnReadyToSendSignalsNetworkState) {
  EXPECT_EQ(webrtc::Call::kNetworkUp, fake_call_->GetNetworkState());

  channel_->OnReadyToSend(false);
  EXPECT_EQ(webrtc::Call::kNetworkDown, fake_call_->GetNetworkState());

  channel_->OnReadyToSend(true);
  EXPECT_EQ(webrtc::Call::kNetworkUp, fake_call_->GetNetworkState());
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
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(send_codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_TRUE(channel_->SetSend(true));

  // Verify that the CpuOveruseObserver is registered and trigger downgrade.
  cricket::VideoOptions options;
  options.cpu_overuse_detection.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));
  // Trigger overuse.
  webrtc::LoadObserver* overuse_callback =
      fake_call_->GetConfig().overuse_callback;
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
  EXPECT_EQ(stats.decode_ms, info.receivers[0].decode_ms);
  EXPECT_EQ(stats.max_decode_ms, info.receivers[0].max_decode_ms);
  EXPECT_EQ(stats.current_delay_ms, info.receivers[0].current_delay_ms);
  EXPECT_EQ(stats.target_delay_ms, info.receivers[0].target_delay_ms);
  EXPECT_EQ(stats.jitter_buffer_ms, info.receivers[0].jitter_buffer_ms);
  EXPECT_EQ(stats.min_playout_delay_ms, info.receivers[0].min_playout_delay_ms);
  EXPECT_EQ(stats.render_delay_ms, info.receivers[0].render_delay_ms);
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
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));

  const std::vector<uint32> ssrcs = MAKE_VECTOR(kSsrcs1);
  const std::vector<uint32> rtx_ssrcs = MAKE_VECTOR(kRtxSsrcs1);

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

class WebRtcVideoEngine2SimulcastTest : public testing::Test {
 public:
  WebRtcVideoEngine2SimulcastTest()
      : engine_(nullptr), engine_codecs_(engine_.codecs()) {
    assert(!engine_codecs_.empty());

    bool codec_set = false;
    for (size_t i = 0; i < engine_codecs_.size(); ++i) {
      if (engine_codecs_[i].name == "red") {
        default_red_codec_ = engine_codecs_[i];
      } else if (engine_codecs_[i].name == "ulpfec") {
        default_ulpfec_codec_ = engine_codecs_[i];
      } else if (engine_codecs_[i].name == "rtx") {
        default_rtx_codec_ = engine_codecs_[i];
      } else if (!codec_set) {
        default_codec_ = engine_codecs_[i];
        codec_set = true;
      }
    }

    assert(codec_set);
  }

 protected:
  WebRtcVideoEngine2 engine_;
  VideoCodec default_codec_;
  VideoCodec default_red_codec_;
  VideoCodec default_ulpfec_codec_;
  VideoCodec default_rtx_codec_;
  // TODO(pbos): Remove engine_codecs_ unless used a lot.
  std::vector<VideoCodec> engine_codecs_;
};

class WebRtcVideoChannel2SimulcastTest : public WebRtcVideoEngine2SimulcastTest,
                                         public WebRtcCallFactory {
 public:
  WebRtcVideoChannel2SimulcastTest() : fake_call_(NULL) {}

  void SetUp() override {
    engine_.SetCallFactory(this);
    engine_.Init(rtc::Thread::Current());
    channel_.reset(engine_.CreateChannel(VideoOptions(), NULL));
    ASSERT_TRUE(fake_call_ != NULL) << "Call not created through factory.";
    last_ssrc_ = 123;
  }

 protected:
  webrtc::Call* CreateCall(const webrtc::Call::Config& config) override {
    assert(fake_call_ == NULL);
    fake_call_ = new FakeCall(config);
    return fake_call_;
  }

  void VerifySimulcastSettings(const VideoCodec& codec,
                               VideoOptions::HighestBitrate bitrate_mode,
                               size_t num_configured_streams,
                               size_t expected_num_streams,
                               SimulcastBitrateMode simulcast_bitrate_mode) {
    cricket::VideoOptions options;
    options.video_highest_bitrate.Set(bitrate_mode);
    EXPECT_TRUE(channel_->SetOptions(options));

    std::vector<VideoCodec> codecs;
    codecs.push_back(codec);
    ASSERT_TRUE(channel_->SetSendCodecs(codecs));

    std::vector<uint32> ssrcs = MAKE_VECTOR(kSsrcs3);
    assert(num_configured_streams <= ssrcs.size());
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
        num_configured_streams,
        simulcast_bitrate_mode,
        codec.width,
        codec.height,
        0,
        kDefaultQpMax,
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
        fake_call_->GetVideoSendStreams().size();
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
    return AddRecvStream(StreamParams::CreateLegacy(last_ssrc_++));
  }

  FakeVideoReceiveStream* AddRecvStream(const StreamParams& sp) {
    size_t num_streams =
        fake_call_->GetVideoReceiveStreams().size();
    EXPECT_TRUE(channel_->AddRecvStream(sp));
    std::vector<FakeVideoReceiveStream*> streams =
        fake_call_->GetVideoReceiveStreams();
    EXPECT_EQ(num_streams + 1, streams.size());
    return streams[streams.size() - 1];
  }

  FakeCall* fake_call_;
  rtc::scoped_ptr<VideoMediaChannel> channel_;
  uint32 last_ssrc_;
};

TEST_F(WebRtcVideoChannel2SimulcastTest, SetSendCodecsWith2SimulcastStreams) {
  VerifySimulcastSettings(kVp8Codec, VideoOptions::NORMAL, 2, 2, SBM_NORMAL);
}

TEST_F(WebRtcVideoChannel2SimulcastTest, SetSendCodecsWith3SimulcastStreams) {
  VerifySimulcastSettings(
      kVp8Codec720p, VideoOptions::NORMAL, 3, 3, SBM_NORMAL);
}

TEST_F(WebRtcVideoChannel2SimulcastTest,
       SetSendCodecsWith2SimulcastStreamsHighBitrateMode) {
  VerifySimulcastSettings(kVp8Codec, VideoOptions::HIGH, 2, 2, SBM_HIGH);
}

TEST_F(WebRtcVideoChannel2SimulcastTest,
       SetSendCodecsWith3SimulcastStreamsHighBitrateMode) {
  VerifySimulcastSettings(kVp8Codec720p, VideoOptions::HIGH, 3, 3, SBM_HIGH);
}

TEST_F(WebRtcVideoChannel2SimulcastTest,
       SetSendCodecsWith2SimulcastStreamsVeryHighBitrateMode) {
  VerifySimulcastSettings(
      kVp8Codec, VideoOptions::VERY_HIGH, 2, 2, SBM_VERY_HIGH);
}

TEST_F(WebRtcVideoChannel2SimulcastTest,
       SetSendCodecsWith3SimulcastStreamsVeryHighBitrateMode) {
  VerifySimulcastSettings(
      kVp8Codec720p, VideoOptions::VERY_HIGH, 3, 3, SBM_VERY_HIGH);
}

// Test that we normalize send codec format size in simulcast.
TEST_F(WebRtcVideoChannel2SimulcastTest, SetSendCodecsWithOddSizeInSimulcast) {
  cricket::VideoCodec codec(kVp8Codec270p);
  codec.width += 1;
  codec.height += 1;
  VerifySimulcastSettings(codec, VideoOptions::NORMAL, 2, 2, SBM_NORMAL);
}

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

TEST_F(WebRtcVideoEngine2SimulcastTest, DISABLED_TestAdaptToCpuLoad) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
}

TEST_F(WebRtcVideoEngine2SimulcastTest, DISABLED_TestAdaptToCpuLoadDisabled) {
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
       DISABLED_DontUseSimulcastAdapterOnNoneVp8Factory) {
  // TODO(pbos): Implement.
  FAIL() << "Not implemented.";
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

TEST_F(WebRtcVideoChannel2SimulcastTest,
       DISABLED_DISABLED_SimulcastSend_480x270) {
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

// Test reset send codec with simulcast.
// Disabled per b/6773425
TEST_F(WebRtcVideoChannel2SimulcastTest,
       DISABLED_DISABLED_SimulcastResetSendCodec) {
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
