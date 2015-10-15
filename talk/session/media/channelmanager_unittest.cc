/*
 * libjingle
 * Copyright 2008 Google Inc.
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

#include "talk/app/webrtc/fakemediacontroller.h"
#include "talk/media/base/fakecapturemanager.h"
#include "talk/media/base/fakemediaengine.h"
#include "talk/media/base/fakevideocapturer.h"
#include "talk/media/base/testutils.h"
#include "talk/media/webrtc/fakewebrtccall.h"
#include "talk/session/media/channelmanager.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/thread.h"
#include "webrtc/p2p/base/faketransportcontroller.h"

namespace cricket {

static const AudioCodec kAudioCodecs[] = {
  AudioCodec(97, "voice", 1, 2, 3, 0),
  AudioCodec(111, "OPUS", 48000, 32000, 2, 0),
};

static const VideoCodec kVideoCodecs[] = {
  VideoCodec(99, "H264", 100, 200, 300, 0),
  VideoCodec(100, "VP8", 100, 200, 300, 0),
  VideoCodec(96, "rtx", 100, 200, 300, 0),
};

class ChannelManagerTest : public testing::Test {
 protected:
  ChannelManagerTest()
      : fme_(new cricket::FakeMediaEngine()),
        fdme_(new cricket::FakeDataEngine()),
        fcm_(new cricket::FakeCaptureManager()),
        cm_(new cricket::ChannelManager(fme_,
                                        fdme_,
                                        fcm_,
                                        rtc::Thread::Current())),
        fake_call_(webrtc::Call::Config()),
        fake_mc_(cm_, &fake_call_),
        transport_controller_(
            new cricket::FakeTransportController(ICEROLE_CONTROLLING)) {}

  virtual void SetUp() {
    fme_->SetAudioCodecs(MAKE_VECTOR(kAudioCodecs));
    fme_->SetVideoCodecs(MAKE_VECTOR(kVideoCodecs));
  }

  virtual void TearDown() {
    delete transport_controller_;
    delete cm_;
    cm_ = NULL;
    fcm_ = NULL;
    fdme_ = NULL;
    fme_ = NULL;
  }

  rtc::Thread worker_;
  cricket::FakeMediaEngine* fme_;
  cricket::FakeDataEngine* fdme_;
  cricket::FakeCaptureManager* fcm_;
  cricket::ChannelManager* cm_;
  cricket::FakeCall fake_call_;
  cricket::FakeMediaController fake_mc_;
  cricket::FakeTransportController* transport_controller_;
};

// Test that we startup/shutdown properly.
TEST_F(ChannelManagerTest, StartupShutdown) {
  EXPECT_FALSE(cm_->initialized());
  EXPECT_EQ(rtc::Thread::Current(), cm_->worker_thread());
  EXPECT_TRUE(cm_->Init());
  EXPECT_TRUE(cm_->initialized());
  cm_->Terminate();
  EXPECT_FALSE(cm_->initialized());
}

// Test that we startup/shutdown properly with a worker thread.
TEST_F(ChannelManagerTest, StartupShutdownOnThread) {
  worker_.Start();
  EXPECT_FALSE(cm_->initialized());
  EXPECT_EQ(rtc::Thread::Current(), cm_->worker_thread());
  EXPECT_TRUE(cm_->set_worker_thread(&worker_));
  EXPECT_EQ(&worker_, cm_->worker_thread());
  EXPECT_TRUE(cm_->Init());
  EXPECT_TRUE(cm_->initialized());
  // Setting the worker thread while initialized should fail.
  EXPECT_FALSE(cm_->set_worker_thread(rtc::Thread::Current()));
  cm_->Terminate();
  EXPECT_FALSE(cm_->initialized());
}

// Test that we can create and destroy a voice and video channel.
TEST_F(ChannelManagerTest, CreateDestroyChannels) {
  EXPECT_TRUE(cm_->Init());
  cricket::VoiceChannel* voice_channel =
      cm_->CreateVoiceChannel(&fake_mc_, transport_controller_,
                              cricket::CN_AUDIO, false, AudioOptions());
  EXPECT_TRUE(voice_channel != nullptr);
  cricket::VideoChannel* video_channel =
      cm_->CreateVideoChannel(&fake_mc_, transport_controller_,
                              cricket::CN_VIDEO, false, VideoOptions());
  EXPECT_TRUE(video_channel != nullptr);
  cricket::DataChannel* data_channel = cm_->CreateDataChannel(
      transport_controller_, cricket::CN_DATA, false, cricket::DCT_RTP);
  EXPECT_TRUE(data_channel != nullptr);
  cm_->DestroyVideoChannel(video_channel);
  cm_->DestroyVoiceChannel(voice_channel);
  cm_->DestroyDataChannel(data_channel);
  cm_->Terminate();
}

// Test that we can create and destroy a voice and video channel with a worker.
TEST_F(ChannelManagerTest, CreateDestroyChannelsOnThread) {
  worker_.Start();
  EXPECT_TRUE(cm_->set_worker_thread(&worker_));
  EXPECT_TRUE(cm_->Init());
  delete transport_controller_;
  transport_controller_ =
      new cricket::FakeTransportController(&worker_, ICEROLE_CONTROLLING);
  cricket::VoiceChannel* voice_channel =
      cm_->CreateVoiceChannel(&fake_mc_, transport_controller_,
                              cricket::CN_AUDIO, false, AudioOptions());
  EXPECT_TRUE(voice_channel != nullptr);
  cricket::VideoChannel* video_channel =
      cm_->CreateVideoChannel(&fake_mc_, transport_controller_,
                              cricket::CN_VIDEO, false, VideoOptions());
  EXPECT_TRUE(video_channel != nullptr);
  cricket::DataChannel* data_channel = cm_->CreateDataChannel(
      transport_controller_, cricket::CN_DATA, false, cricket::DCT_RTP);
  EXPECT_TRUE(data_channel != nullptr);
  cm_->DestroyVideoChannel(video_channel);
  cm_->DestroyVoiceChannel(voice_channel);
  cm_->DestroyDataChannel(data_channel);
  cm_->Terminate();
}

// Test that we fail to create a voice/video channel if the session is unable
// to create a cricket::TransportChannel
TEST_F(ChannelManagerTest, NoTransportChannelTest) {
  EXPECT_TRUE(cm_->Init());
  transport_controller_->set_fail_channel_creation(true);
  // The test is useless unless the session does not fail creating
  // cricket::TransportChannel.
  ASSERT_TRUE(transport_controller_->CreateTransportChannel_w(
                  "audio", cricket::ICE_CANDIDATE_COMPONENT_RTP) == nullptr);

  cricket::VoiceChannel* voice_channel =
      cm_->CreateVoiceChannel(&fake_mc_, transport_controller_,
                              cricket::CN_AUDIO, false, AudioOptions());
  EXPECT_TRUE(voice_channel == nullptr);
  cricket::VideoChannel* video_channel =
      cm_->CreateVideoChannel(&fake_mc_, transport_controller_,
                              cricket::CN_VIDEO, false, VideoOptions());
  EXPECT_TRUE(video_channel == nullptr);
  cricket::DataChannel* data_channel = cm_->CreateDataChannel(
      transport_controller_, cricket::CN_DATA, false, cricket::DCT_RTP);
  EXPECT_TRUE(data_channel == nullptr);
  cm_->Terminate();
}

// Test that SetDefaultVideoCodec passes through the right values.
TEST_F(ChannelManagerTest, SetDefaultVideoEncoderConfig) {
  cricket::VideoCodec codec(96, "G264", 1280, 720, 60, 0);
  cricket::VideoEncoderConfig config(codec, 1, 2);
  EXPECT_TRUE(cm_->Init());
  EXPECT_TRUE(cm_->SetDefaultVideoEncoderConfig(config));
  EXPECT_EQ(config, fme_->default_video_encoder_config());
}

struct GetCapturerFrameSize : public sigslot::has_slots<> {
  void OnVideoFrame(VideoCapturer* capturer, const VideoFrame* frame) {
    width = frame->GetWidth();
    height = frame->GetHeight();
  }
  GetCapturerFrameSize(VideoCapturer* capturer) : width(0), height(0) {
    capturer->SignalVideoFrame.connect(this,
                                       &GetCapturerFrameSize::OnVideoFrame);
    static_cast<FakeVideoCapturer*>(capturer)->CaptureFrame();
  }
  size_t width;
  size_t height;
};

// Test that SetDefaultVideoCodec passes through the right values.
TEST_F(ChannelManagerTest, SetDefaultVideoCodecBeforeInit) {
  cricket::VideoCodec codec(96, "G264", 1280, 720, 60, 0);
  cricket::VideoEncoderConfig config(codec, 1, 2);
  EXPECT_TRUE(cm_->SetDefaultVideoEncoderConfig(config));
  EXPECT_TRUE(cm_->Init());
  EXPECT_EQ(config, fme_->default_video_encoder_config());
}

TEST_F(ChannelManagerTest, GetSetOutputVolumeBeforeInit) {
  int level;
  // Before init, SetOutputVolume() remembers the volume but does not change the
  // volume of the engine. GetOutputVolume() should fail.
  EXPECT_EQ(-1, fme_->output_volume());
  EXPECT_FALSE(cm_->GetOutputVolume(&level));
  EXPECT_FALSE(cm_->SetOutputVolume(-1));  // Invalid volume.
  EXPECT_TRUE(cm_->SetOutputVolume(99));
  EXPECT_EQ(-1, fme_->output_volume());

  // Init() will apply the remembered volume.
  EXPECT_TRUE(cm_->Init());
  EXPECT_TRUE(cm_->GetOutputVolume(&level));
  EXPECT_EQ(99, level);
  EXPECT_EQ(level, fme_->output_volume());

  EXPECT_TRUE(cm_->SetOutputVolume(60));
  EXPECT_TRUE(cm_->GetOutputVolume(&level));
  EXPECT_EQ(60, level);
  EXPECT_EQ(level, fme_->output_volume());
}

TEST_F(ChannelManagerTest, GetSetOutputVolume) {
  int level;
  EXPECT_TRUE(cm_->Init());
  EXPECT_TRUE(cm_->GetOutputVolume(&level));
  EXPECT_EQ(level, fme_->output_volume());

  EXPECT_FALSE(cm_->SetOutputVolume(-1));  // Invalid volume.
  EXPECT_TRUE(cm_->SetOutputVolume(60));
  EXPECT_EQ(60, fme_->output_volume());
  EXPECT_TRUE(cm_->GetOutputVolume(&level));
  EXPECT_EQ(60, level);
}

// Test that logging options set before Init are applied properly,
// and retained even after Init.
TEST_F(ChannelManagerTest, SetLoggingBeforeInit) {
  cm_->SetVoiceLogging(rtc::LS_INFO, "test-voice");
  cm_->SetVideoLogging(rtc::LS_VERBOSE, "test-video");
  EXPECT_EQ(rtc::LS_INFO, fme_->voice_loglevel());
  EXPECT_STREQ("test-voice", fme_->voice_logfilter().c_str());
  EXPECT_EQ(rtc::LS_VERBOSE, fme_->video_loglevel());
  EXPECT_STREQ("test-video", fme_->video_logfilter().c_str());
  EXPECT_TRUE(cm_->Init());
  EXPECT_EQ(rtc::LS_INFO, fme_->voice_loglevel());
  EXPECT_STREQ("test-voice", fme_->voice_logfilter().c_str());
  EXPECT_EQ(rtc::LS_VERBOSE, fme_->video_loglevel());
  EXPECT_STREQ("test-video", fme_->video_logfilter().c_str());
}

// Test that logging options set after Init are applied properly.
TEST_F(ChannelManagerTest, SetLogging) {
  EXPECT_TRUE(cm_->Init());
  cm_->SetVoiceLogging(rtc::LS_INFO, "test-voice");
  cm_->SetVideoLogging(rtc::LS_VERBOSE, "test-video");
  EXPECT_EQ(rtc::LS_INFO, fme_->voice_loglevel());
  EXPECT_STREQ("test-voice", fme_->voice_logfilter().c_str());
  EXPECT_EQ(rtc::LS_VERBOSE, fme_->video_loglevel());
  EXPECT_STREQ("test-video", fme_->video_logfilter().c_str());
}

TEST_F(ChannelManagerTest, SetVideoRtxEnabled) {
  std::vector<VideoCodec> codecs;
  const VideoCodec rtx_codec(96, "rtx", 0, 0, 0, 0);

  // By default RTX is disabled.
  cm_->GetSupportedVideoCodecs(&codecs);
  EXPECT_FALSE(ContainsMatchingCodec(codecs, rtx_codec));

  // Enable and check.
  EXPECT_TRUE(cm_->SetVideoRtxEnabled(true));
  cm_->GetSupportedVideoCodecs(&codecs);
  EXPECT_TRUE(ContainsMatchingCodec(codecs, rtx_codec));

  // Disable and check.
  EXPECT_TRUE(cm_->SetVideoRtxEnabled(false));
  cm_->GetSupportedVideoCodecs(&codecs);
  EXPECT_FALSE(ContainsMatchingCodec(codecs, rtx_codec));

  // Cannot toggle rtx after initialization.
  EXPECT_TRUE(cm_->Init());
  EXPECT_FALSE(cm_->SetVideoRtxEnabled(true));
  EXPECT_FALSE(cm_->SetVideoRtxEnabled(false));

  // Can set again after terminate.
  cm_->Terminate();
  EXPECT_TRUE(cm_->SetVideoRtxEnabled(true));
  cm_->GetSupportedVideoCodecs(&codecs);
  EXPECT_TRUE(ContainsMatchingCodec(codecs, rtx_codec));
}

}  // namespace cricket
