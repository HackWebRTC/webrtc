// libjingle
// Copyright 2008 Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. The name of the author may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "talk/base/gunit.h"
#include "talk/base/logging.h"
#include "talk/base/thread.h"
#include "talk/media/base/fakecapturemanager.h"
#include "talk/media/base/fakemediaengine.h"
#include "talk/media/base/fakemediaprocessor.h"
#include "talk/media/base/nullvideorenderer.h"
#include "talk/media/devices/fakedevicemanager.h"
#include "talk/media/base/testutils.h"
#include "talk/p2p/base/fakesession.h"
#include "talk/session/media/channelmanager.h"

namespace cricket {

static const AudioCodec kAudioCodecs[] = {
  AudioCodec(97, "voice", 1, 2, 3, 0),
  AudioCodec(110, "CELT", 32000, 48000, 2, 0),
  AudioCodec(111, "OPUS", 48000, 32000, 2, 0),
};

static const VideoCodec kVideoCodecs[] = {
  VideoCodec(99, "H264", 100, 200, 300, 0),
  VideoCodec(100, "VP8", 100, 200, 300, 0),
  VideoCodec(96, "rtx", 100, 200, 300, 0),
};

class ChannelManagerTest : public testing::Test {
 protected:
  ChannelManagerTest() : fme_(NULL), fdm_(NULL), fcm_(NULL), cm_(NULL) {
  }

  virtual void SetUp() {
    fme_ = new cricket::FakeMediaEngine();
    fme_->SetAudioCodecs(MAKE_VECTOR(kAudioCodecs));
    fme_->SetVideoCodecs(MAKE_VECTOR(kVideoCodecs));
    fdme_ = new cricket::FakeDataEngine();
    fdm_ = new cricket::FakeDeviceManager();
    fcm_ = new cricket::FakeCaptureManager();
    cm_ = new cricket::ChannelManager(
        fme_, fdme_, fdm_, fcm_, talk_base::Thread::Current());
    session_ = new cricket::FakeSession(true);

    std::vector<std::string> in_device_list, out_device_list, vid_device_list;
    in_device_list.push_back("audio-in1");
    in_device_list.push_back("audio-in2");
    out_device_list.push_back("audio-out1");
    out_device_list.push_back("audio-out2");
    vid_device_list.push_back("video-in1");
    vid_device_list.push_back("video-in2");
    fdm_->SetAudioInputDevices(in_device_list);
    fdm_->SetAudioOutputDevices(out_device_list);
    fdm_->SetVideoCaptureDevices(vid_device_list);
  }

  virtual void TearDown() {
    delete session_;
    delete cm_;
    cm_ = NULL;
    fdm_ = NULL;
    fcm_ = NULL;
    fdme_ = NULL;
    fme_ = NULL;
  }

  talk_base::Thread worker_;
  cricket::FakeMediaEngine* fme_;
  cricket::FakeDataEngine* fdme_;
  cricket::FakeDeviceManager* fdm_;
  cricket::FakeCaptureManager* fcm_;
  cricket::ChannelManager* cm_;
  cricket::FakeSession* session_;
};

// Test that we startup/shutdown properly.
TEST_F(ChannelManagerTest, StartupShutdown) {
  EXPECT_FALSE(cm_->initialized());
  EXPECT_EQ(talk_base::Thread::Current(), cm_->worker_thread());
  EXPECT_TRUE(cm_->Init());
  EXPECT_TRUE(cm_->initialized());
  cm_->Terminate();
  EXPECT_FALSE(cm_->initialized());
}

// Test that we startup/shutdown properly with a worker thread.
TEST_F(ChannelManagerTest, StartupShutdownOnThread) {
  worker_.Start();
  EXPECT_FALSE(cm_->initialized());
  EXPECT_EQ(talk_base::Thread::Current(), cm_->worker_thread());
  EXPECT_TRUE(cm_->set_worker_thread(&worker_));
  EXPECT_EQ(&worker_, cm_->worker_thread());
  EXPECT_TRUE(cm_->Init());
  EXPECT_TRUE(cm_->initialized());
  // Setting the worker thread while initialized should fail.
  EXPECT_FALSE(cm_->set_worker_thread(talk_base::Thread::Current()));
  cm_->Terminate();
  EXPECT_FALSE(cm_->initialized());
}

// Test that we fail to startup if we're given an unstarted thread.
TEST_F(ChannelManagerTest, StartupShutdownOnUnstartedThread) {
  EXPECT_TRUE(cm_->set_worker_thread(&worker_));
  EXPECT_FALSE(cm_->Init());
  EXPECT_FALSE(cm_->initialized());
}

// Test that we can create and destroy a voice and video channel.
TEST_F(ChannelManagerTest, CreateDestroyChannels) {
  EXPECT_TRUE(cm_->Init());
  cricket::VoiceChannel* voice_channel = cm_->CreateVoiceChannel(
      session_, cricket::CN_AUDIO, false);
  EXPECT_TRUE(voice_channel != NULL);
  cricket::VideoChannel* video_channel =
      cm_->CreateVideoChannel(session_, cricket::CN_VIDEO,
                              false, voice_channel);
  EXPECT_TRUE(video_channel != NULL);
  cricket::DataChannel* data_channel =
      cm_->CreateDataChannel(session_, cricket::CN_DATA,
                             false, cricket::DCT_RTP);
  EXPECT_TRUE(data_channel != NULL);
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
  cricket::VoiceChannel* voice_channel = cm_->CreateVoiceChannel(
      session_, cricket::CN_AUDIO, false);
  EXPECT_TRUE(voice_channel != NULL);
  cricket::VideoChannel* video_channel =
      cm_->CreateVideoChannel(session_, cricket::CN_VIDEO,
                              false, voice_channel);
  EXPECT_TRUE(video_channel != NULL);
  cricket::DataChannel* data_channel =
      cm_->CreateDataChannel(session_, cricket::CN_DATA,
                             false, cricket::DCT_RTP);
  EXPECT_TRUE(data_channel != NULL);
  cm_->DestroyVideoChannel(video_channel);
  cm_->DestroyVoiceChannel(voice_channel);
  cm_->DestroyDataChannel(data_channel);
  cm_->Terminate();
}

// Test that we fail to create a voice/video channel if the session is unable
// to create a cricket::TransportChannel
TEST_F(ChannelManagerTest, NoTransportChannelTest) {
  EXPECT_TRUE(cm_->Init());
  session_->set_fail_channel_creation(true);
  // The test is useless unless the session does not fail creating
  // cricket::TransportChannel.
  ASSERT_TRUE(session_->CreateChannel(
      "audio", "rtp", cricket::ICE_CANDIDATE_COMPONENT_RTP) == NULL);

  cricket::VoiceChannel* voice_channel = cm_->CreateVoiceChannel(
      session_, cricket::CN_AUDIO, false);
  EXPECT_TRUE(voice_channel == NULL);
  cricket::VideoChannel* video_channel =
      cm_->CreateVideoChannel(session_, cricket::CN_VIDEO,
                              false, voice_channel);
  EXPECT_TRUE(video_channel == NULL);
  cricket::DataChannel* data_channel =
      cm_->CreateDataChannel(session_, cricket::CN_DATA,
                             false, cricket::DCT_RTP);
  EXPECT_TRUE(data_channel == NULL);
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

// Test that SetDefaultVideoCodec passes through the right values.
TEST_F(ChannelManagerTest, SetDefaultVideoCodecBeforeInit) {
  cricket::VideoCodec codec(96, "G264", 1280, 720, 60, 0);
  cricket::VideoEncoderConfig config(codec, 1, 2);
  EXPECT_TRUE(cm_->SetDefaultVideoEncoderConfig(config));
  EXPECT_TRUE(cm_->Init());
  EXPECT_EQ(config, fme_->default_video_encoder_config());
}

TEST_F(ChannelManagerTest, SetAudioOptionsBeforeInit) {
  // Test that values that we set before Init are applied.
  EXPECT_TRUE(cm_->SetAudioOptions("audio-in1", "audio-out1", 0x2));
  EXPECT_TRUE(cm_->Init());
  EXPECT_EQ("audio-in1", fme_->audio_in_device());
  EXPECT_EQ("audio-out1", fme_->audio_out_device());
  EXPECT_EQ(0x2, fme_->audio_options());
  EXPECT_EQ(0, fme_->audio_delay_offset());
  EXPECT_EQ(cricket::MediaEngineInterface::kDefaultAudioDelayOffset,
            fme_->audio_delay_offset());
}

TEST_F(ChannelManagerTest, GetAudioOptionsBeforeInit) {
  std::string audio_in, audio_out;
  int opts;
  // Test that GetAudioOptions works before Init.
  EXPECT_TRUE(cm_->SetAudioOptions("audio-in2", "audio-out2", 0x1));
  EXPECT_TRUE(cm_->GetAudioOptions(&audio_in, &audio_out, &opts));
  EXPECT_EQ("audio-in2", audio_in);
  EXPECT_EQ("audio-out2", audio_out);
  EXPECT_EQ(0x1, opts);
  // Test that options set before Init can be gotten after Init.
  EXPECT_TRUE(cm_->SetAudioOptions("audio-in1", "audio-out1", 0x2));
  EXPECT_TRUE(cm_->Init());
  EXPECT_TRUE(cm_->GetAudioOptions(&audio_in, &audio_out, &opts));
  EXPECT_EQ("audio-in1", audio_in);
  EXPECT_EQ("audio-out1", audio_out);
  EXPECT_EQ(0x2, opts);
}

TEST_F(ChannelManagerTest, GetAudioOptionsWithNullParameters) {
  std::string audio_in, audio_out;
  int opts;
  EXPECT_TRUE(cm_->SetAudioOptions("audio-in2", "audio-out2", 0x1));
  EXPECT_TRUE(cm_->GetAudioOptions(&audio_in, NULL, NULL));
  EXPECT_EQ("audio-in2", audio_in);
  EXPECT_TRUE(cm_->GetAudioOptions(NULL, &audio_out, NULL));
  EXPECT_EQ("audio-out2", audio_out);
  EXPECT_TRUE(cm_->GetAudioOptions(NULL, NULL, &opts));
  EXPECT_EQ(0x1, opts);
}

TEST_F(ChannelManagerTest, SetAudioOptions) {
  // Test initial state.
  EXPECT_TRUE(cm_->Init());
  EXPECT_EQ(std::string(cricket::DeviceManagerInterface::kDefaultDeviceName),
            fme_->audio_in_device());
  EXPECT_EQ(std::string(cricket::DeviceManagerInterface::kDefaultDeviceName),
            fme_->audio_out_device());
  EXPECT_EQ(cricket::MediaEngineInterface::DEFAULT_AUDIO_OPTIONS,
            fme_->audio_options());
  EXPECT_EQ(cricket::MediaEngineInterface::kDefaultAudioDelayOffset,
            fme_->audio_delay_offset());
  // Test setting defaults.
  EXPECT_TRUE(cm_->SetAudioOptions("", "",
      cricket::MediaEngineInterface::DEFAULT_AUDIO_OPTIONS));
  EXPECT_EQ("", fme_->audio_in_device());
  EXPECT_EQ("", fme_->audio_out_device());
  EXPECT_EQ(cricket::MediaEngineInterface::DEFAULT_AUDIO_OPTIONS,
            fme_->audio_options());
  EXPECT_EQ(cricket::MediaEngineInterface::kDefaultAudioDelayOffset,
            fme_->audio_delay_offset());
  // Test setting specific values.
  EXPECT_TRUE(cm_->SetAudioOptions("audio-in1", "audio-out1", 0x2));
  EXPECT_EQ("audio-in1", fme_->audio_in_device());
  EXPECT_EQ("audio-out1", fme_->audio_out_device());
  EXPECT_EQ(0x2, fme_->audio_options());
  EXPECT_EQ(cricket::MediaEngineInterface::kDefaultAudioDelayOffset,
            fme_->audio_delay_offset());
  // Test setting bad values.
  EXPECT_FALSE(cm_->SetAudioOptions("audio-in9", "audio-out2", 0x1));
}

TEST_F(ChannelManagerTest, GetAudioOptions) {
  std::string audio_in, audio_out;
  int opts;
  // Test initial state.
  EXPECT_TRUE(cm_->Init());
  EXPECT_TRUE(cm_->GetAudioOptions(&audio_in, &audio_out, &opts));
  EXPECT_EQ(std::string(cricket::DeviceManagerInterface::kDefaultDeviceName),
            audio_in);
  EXPECT_EQ(std::string(cricket::DeviceManagerInterface::kDefaultDeviceName),
            audio_out);
  EXPECT_EQ(cricket::MediaEngineInterface::DEFAULT_AUDIO_OPTIONS, opts);
  // Test that we get back specific values that we set.
  EXPECT_TRUE(cm_->SetAudioOptions("audio-in1", "audio-out1", 0x2));
  EXPECT_TRUE(cm_->GetAudioOptions(&audio_in, &audio_out, &opts));
  EXPECT_EQ("audio-in1", audio_in);
  EXPECT_EQ("audio-out1", audio_out);
  EXPECT_EQ(0x2, opts);
}

TEST_F(ChannelManagerTest, SetCaptureDeviceBeforeInit) {
  // Test that values that we set before Init are applied.
  EXPECT_TRUE(cm_->SetCaptureDevice("video-in2"));
  EXPECT_TRUE(cm_->Init());
  EXPECT_EQ("video-in2", cm_->video_device_name());
}

TEST_F(ChannelManagerTest, GetCaptureDeviceBeforeInit) {
  std::string video_in;
  // Test that GetCaptureDevice works before Init.
  EXPECT_TRUE(cm_->SetCaptureDevice("video-in1"));
  EXPECT_TRUE(cm_->GetCaptureDevice(&video_in));
  EXPECT_EQ("video-in1", video_in);
  // Test that options set before Init can be gotten after Init.
  EXPECT_TRUE(cm_->SetCaptureDevice("video-in2"));
  EXPECT_TRUE(cm_->Init());
  EXPECT_TRUE(cm_->GetCaptureDevice(&video_in));
  EXPECT_EQ("video-in2", video_in);
}

TEST_F(ChannelManagerTest, SetCaptureDevice) {
  // Test setting defaults.
  EXPECT_TRUE(cm_->Init());
  EXPECT_TRUE(cm_->SetCaptureDevice(""));  // will use DeviceManager default
  EXPECT_EQ("video-in1", cm_->video_device_name());
  // Test setting specific values.
  EXPECT_TRUE(cm_->SetCaptureDevice("video-in2"));
  EXPECT_EQ("video-in2", cm_->video_device_name());
  // TODO(juberti): Add test for invalid value here.
}

// Test unplugging and plugging back the preferred devices. When the preferred
// device is unplugged, we fall back to the default device. When the preferred
// device is plugged back, we use it.
TEST_F(ChannelManagerTest, SetAudioOptionsUnplugPlug) {
  // Set preferences "audio-in1" and "audio-out1" before init.
  EXPECT_TRUE(cm_->SetAudioOptions("audio-in1", "audio-out1", 0x2));
  // Unplug device "audio-in1" and "audio-out1".
  std::vector<std::string> in_device_list, out_device_list;
  in_device_list.push_back("audio-in2");
  out_device_list.push_back("audio-out2");
  fdm_->SetAudioInputDevices(in_device_list);
  fdm_->SetAudioOutputDevices(out_device_list);
  // Init should fall back to default devices.
  EXPECT_TRUE(cm_->Init());
  // The media engine should use the default.
  EXPECT_EQ("", fme_->audio_in_device());
  EXPECT_EQ("", fme_->audio_out_device());
  // The channel manager keeps the preferences "audio-in1" and "audio-out1".
  std::string audio_in, audio_out;
  EXPECT_TRUE(cm_->GetAudioOptions(&audio_in, &audio_out, NULL));
  EXPECT_EQ("audio-in1", audio_in);
  EXPECT_EQ("audio-out1", audio_out);
  cm_->Terminate();

  // Plug devices "audio-in2" and "audio-out2" back.
  in_device_list.push_back("audio-in1");
  out_device_list.push_back("audio-out1");
  fdm_->SetAudioInputDevices(in_device_list);
  fdm_->SetAudioOutputDevices(out_device_list);
  // Init again. The preferences, "audio-in2" and "audio-out2", are used.
  EXPECT_TRUE(cm_->Init());
  EXPECT_EQ("audio-in1", fme_->audio_in_device());
  EXPECT_EQ("audio-out1", fme_->audio_out_device());
  EXPECT_TRUE(cm_->GetAudioOptions(&audio_in, &audio_out, NULL));
  EXPECT_EQ("audio-in1", audio_in);
  EXPECT_EQ("audio-out1", audio_out);
}

// We have one camera. Unplug it, fall back to no camera.
TEST_F(ChannelManagerTest, SetCaptureDeviceUnplugPlugOneCamera) {
  // Set preferences "video-in1" before init.
  std::vector<std::string> vid_device_list;
  vid_device_list.push_back("video-in1");
  fdm_->SetVideoCaptureDevices(vid_device_list);
  EXPECT_TRUE(cm_->SetCaptureDevice("video-in1"));

  // Unplug "video-in1".
  vid_device_list.clear();
  fdm_->SetVideoCaptureDevices(vid_device_list);

  // Init should fall back to avatar.
  EXPECT_TRUE(cm_->Init());
  // The media engine should use no camera.
  EXPECT_EQ("", cm_->video_device_name());
  // The channel manager keeps the user preference "video-in".
  std::string video_in;
  EXPECT_TRUE(cm_->GetCaptureDevice(&video_in));
  EXPECT_EQ("video-in1", video_in);
  cm_->Terminate();

  // Plug device "video-in1" back.
  vid_device_list.push_back("video-in1");
  fdm_->SetVideoCaptureDevices(vid_device_list);
  // Init again. The user preferred device, "video-in1", is used.
  EXPECT_TRUE(cm_->Init());
  EXPECT_EQ("video-in1", cm_->video_device_name());
  EXPECT_TRUE(cm_->GetCaptureDevice(&video_in));
  EXPECT_EQ("video-in1", video_in);
}

// We have multiple cameras. Unplug the preferred, fall back to another camera.
TEST_F(ChannelManagerTest, SetCaptureDeviceUnplugPlugTwoDevices) {
  // Set video device to "video-in1" before init.
  EXPECT_TRUE(cm_->SetCaptureDevice("video-in1"));
  // Unplug device "video-in1".
  std::vector<std::string> vid_device_list;
  vid_device_list.push_back("video-in2");
  fdm_->SetVideoCaptureDevices(vid_device_list);
  // Init should fall back to default device "video-in2".
  EXPECT_TRUE(cm_->Init());
  // The media engine should use the default device "video-in2".
  EXPECT_EQ("video-in2", cm_->video_device_name());
  // The channel manager keeps the user preference "video-in".
  std::string video_in;
  EXPECT_TRUE(cm_->GetCaptureDevice(&video_in));
  EXPECT_EQ("video-in1", video_in);
  cm_->Terminate();

  // Plug device "video-in1" back.
  vid_device_list.push_back("video-in1");
  fdm_->SetVideoCaptureDevices(vid_device_list);
  // Init again. The user preferred device, "video-in1", is used.
  EXPECT_TRUE(cm_->Init());
  EXPECT_EQ("video-in1", cm_->video_device_name());
  EXPECT_TRUE(cm_->GetCaptureDevice(&video_in));
  EXPECT_EQ("video-in1", video_in);
}

TEST_F(ChannelManagerTest, GetCaptureDevice) {
  std::string video_in;
  // Test setting/getting defaults.
  EXPECT_TRUE(cm_->Init());
  EXPECT_TRUE(cm_->SetCaptureDevice(""));
  EXPECT_TRUE(cm_->GetCaptureDevice(&video_in));
  EXPECT_EQ("video-in1", video_in);
  // Test setting/getting specific values.
  EXPECT_TRUE(cm_->SetCaptureDevice("video-in2"));
  EXPECT_TRUE(cm_->GetCaptureDevice(&video_in));
  EXPECT_EQ("video-in2", video_in);
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

// Test that a value set before Init is applied properly.
TEST_F(ChannelManagerTest, SetLocalRendererBeforeInit) {
  cricket::NullVideoRenderer renderer;
  EXPECT_TRUE(cm_->SetLocalRenderer(&renderer));
  EXPECT_TRUE(cm_->Init());
  EXPECT_EQ(&renderer, fme_->local_renderer());
}

// Test that a value set after init is passed through properly.
TEST_F(ChannelManagerTest, SetLocalRenderer) {
  cricket::NullVideoRenderer renderer;
  EXPECT_TRUE(cm_->Init());
  EXPECT_TRUE(cm_->SetLocalRenderer(&renderer));
  EXPECT_EQ(&renderer, fme_->local_renderer());
}

// Test that logging options set before Init are applied properly,
// and retained even after Init.
TEST_F(ChannelManagerTest, SetLoggingBeforeInit) {
  cm_->SetVoiceLogging(talk_base::LS_INFO, "test-voice");
  cm_->SetVideoLogging(talk_base::LS_VERBOSE, "test-video");
  EXPECT_EQ(talk_base::LS_INFO, fme_->voice_loglevel());
  EXPECT_STREQ("test-voice", fme_->voice_logfilter().c_str());
  EXPECT_EQ(talk_base::LS_VERBOSE, fme_->video_loglevel());
  EXPECT_STREQ("test-video", fme_->video_logfilter().c_str());
  EXPECT_TRUE(cm_->Init());
  EXPECT_EQ(talk_base::LS_INFO, fme_->voice_loglevel());
  EXPECT_STREQ("test-voice", fme_->voice_logfilter().c_str());
  EXPECT_EQ(talk_base::LS_VERBOSE, fme_->video_loglevel());
  EXPECT_STREQ("test-video", fme_->video_logfilter().c_str());
}

// Test that logging options set after Init are applied properly.
TEST_F(ChannelManagerTest, SetLogging) {
  EXPECT_TRUE(cm_->Init());
  cm_->SetVoiceLogging(talk_base::LS_INFO, "test-voice");
  cm_->SetVideoLogging(talk_base::LS_VERBOSE, "test-video");
  EXPECT_EQ(talk_base::LS_INFO, fme_->voice_loglevel());
  EXPECT_STREQ("test-voice", fme_->voice_logfilter().c_str());
  EXPECT_EQ(talk_base::LS_VERBOSE, fme_->video_loglevel());
  EXPECT_STREQ("test-video", fme_->video_logfilter().c_str());
}

// Test that SetVideoCapture passes through the right value.
TEST_F(ChannelManagerTest, SetVideoCapture) {
  // Should fail until we are initialized.
  EXPECT_FALSE(fme_->capture());
  EXPECT_FALSE(cm_->SetVideoCapture(true));
  EXPECT_FALSE(fme_->capture());
  EXPECT_TRUE(cm_->Init());
  EXPECT_FALSE(fme_->capture());
  EXPECT_TRUE(cm_->SetVideoCapture(true));
  EXPECT_TRUE(fme_->capture());
  EXPECT_TRUE(cm_->SetVideoCapture(false));
  EXPECT_FALSE(fme_->capture());
}

// Test that the Video/Voice Processors register and unregister
TEST_F(ChannelManagerTest, RegisterProcessors) {
  cricket::FakeMediaProcessor fmp;
  EXPECT_TRUE(cm_->Init());
  EXPECT_FALSE(fme_->voice_processor_registered(cricket::MPD_TX));
  EXPECT_FALSE(fme_->voice_processor_registered(cricket::MPD_RX));

  EXPECT_FALSE(fme_->voice_processor_registered(cricket::MPD_TX));
  EXPECT_FALSE(fme_->voice_processor_registered(cricket::MPD_RX));

  EXPECT_FALSE(fme_->voice_processor_registered(cricket::MPD_TX));
  EXPECT_FALSE(fme_->voice_processor_registered(cricket::MPD_RX));

  EXPECT_TRUE(cm_->RegisterVoiceProcessor(1,
                                          &fmp,
                                          cricket::MPD_RX));
  EXPECT_FALSE(fme_->voice_processor_registered(cricket::MPD_TX));
  EXPECT_TRUE(fme_->voice_processor_registered(cricket::MPD_RX));


  EXPECT_TRUE(cm_->UnregisterVoiceProcessor(1,
                                            &fmp,
                                            cricket::MPD_RX));
  EXPECT_FALSE(fme_->voice_processor_registered(cricket::MPD_TX));
  EXPECT_FALSE(fme_->voice_processor_registered(cricket::MPD_RX));

  EXPECT_TRUE(cm_->RegisterVoiceProcessor(1,
                                          &fmp,
                                          cricket::MPD_TX));
  EXPECT_TRUE(fme_->voice_processor_registered(cricket::MPD_TX));
  EXPECT_FALSE(fme_->voice_processor_registered(cricket::MPD_RX));

  EXPECT_TRUE(cm_->UnregisterVoiceProcessor(1,
                                            &fmp,
                                            cricket::MPD_TX));
  EXPECT_FALSE(fme_->voice_processor_registered(cricket::MPD_TX));
  EXPECT_FALSE(fme_->voice_processor_registered(cricket::MPD_RX));
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
