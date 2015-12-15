/*
 * libjingle
 * Copyright 2012 Google Inc.
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

#include <string>
#include <utility>

#include "talk/app/webrtc/audiotrack.h"
#include "talk/app/webrtc/mediastream.h"
#include "talk/app/webrtc/remoteaudiosource.h"
#include "talk/app/webrtc/rtpreceiver.h"
#include "talk/app/webrtc/rtpsender.h"
#include "talk/app/webrtc/streamcollection.h"
#include "talk/app/webrtc/videosource.h"
#include "talk/app/webrtc/videotrack.h"
#include "talk/media/base/fakevideocapturer.h"
#include "talk/media/base/mediachannel.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/gunit.h"

using ::testing::_;
using ::testing::Exactly;

static const char kStreamLabel1[] = "local_stream_1";
static const char kVideoTrackId[] = "video_1";
static const char kAudioTrackId[] = "audio_1";
static const uint32_t kVideoSsrc = 98;
static const uint32_t kVideoSsrc2 = 100;
static const uint32_t kAudioSsrc = 99;
static const uint32_t kAudioSsrc2 = 101;

namespace webrtc {

// Helper class to test RtpSender/RtpReceiver.
class MockAudioProvider : public AudioProviderInterface {
 public:
  ~MockAudioProvider() override {}

  MOCK_METHOD2(SetAudioPlayout,
               void(uint32_t ssrc,
                    bool enable));
  MOCK_METHOD4(SetAudioSend,
               void(uint32_t ssrc,
                    bool enable,
                    const cricket::AudioOptions& options,
                    cricket::AudioRenderer* renderer));
  MOCK_METHOD2(SetAudioPlayoutVolume, void(uint32_t ssrc, double volume));

  void SetRawAudioSink(uint32_t,
                       rtc::scoped_ptr<AudioSinkInterface> sink) override {
    sink_ = std::move(sink);
  }

 private:
  rtc::scoped_ptr<AudioSinkInterface> sink_;
};

// Helper class to test RtpSender/RtpReceiver.
class MockVideoProvider : public VideoProviderInterface {
 public:
  virtual ~MockVideoProvider() {}
  MOCK_METHOD2(SetCaptureDevice,
               bool(uint32_t ssrc, cricket::VideoCapturer* camera));
  MOCK_METHOD3(SetVideoPlayout,
               void(uint32_t ssrc,
                    bool enable,
                    cricket::VideoRenderer* renderer));
  MOCK_METHOD3(SetVideoSend,
               void(uint32_t ssrc,
                    bool enable,
                    const cricket::VideoOptions* options));
};

class FakeVideoSource : public Notifier<VideoSourceInterface> {
 public:
  static rtc::scoped_refptr<FakeVideoSource> Create(bool remote) {
    return new rtc::RefCountedObject<FakeVideoSource>(remote);
  }
  virtual cricket::VideoCapturer* GetVideoCapturer() { return &fake_capturer_; }
  virtual void Stop() {}
  virtual void Restart() {}
  virtual void AddSink(cricket::VideoRenderer* output) {}
  virtual void RemoveSink(cricket::VideoRenderer* output) {}
  virtual SourceState state() const { return state_; }
  virtual bool remote() const { return remote_; }
  virtual const cricket::VideoOptions* options() const { return &options_; }
  virtual cricket::VideoRenderer* FrameInput() { return NULL; }

 protected:
  explicit FakeVideoSource(bool remote) : state_(kLive), remote_(remote) {}
  ~FakeVideoSource() {}

 private:
  cricket::FakeVideoCapturer fake_capturer_;
  SourceState state_;
  bool remote_;
  cricket::VideoOptions options_;
};

class RtpSenderReceiverTest : public testing::Test {
 public:
  virtual void SetUp() {
    stream_ = MediaStream::Create(kStreamLabel1);
  }

  void AddVideoTrack(bool remote) {
    rtc::scoped_refptr<VideoSourceInterface> source(
        FakeVideoSource::Create(remote));
    video_track_ = VideoTrack::Create(kVideoTrackId, source);
    EXPECT_TRUE(stream_->AddTrack(video_track_));
  }

  void CreateAudioRtpSender() {
    audio_track_ = AudioTrack::Create(kAudioTrackId, NULL);
    EXPECT_TRUE(stream_->AddTrack(audio_track_));
    EXPECT_CALL(audio_provider_, SetAudioSend(kAudioSsrc, true, _, _));
    audio_rtp_sender_ =
        new AudioRtpSender(stream_->GetAudioTracks()[0], stream_->label(),
                           &audio_provider_, nullptr);
    audio_rtp_sender_->SetSsrc(kAudioSsrc);
  }

  void CreateVideoRtpSender() {
    AddVideoTrack(false);
    EXPECT_CALL(video_provider_,
                SetCaptureDevice(
                    kVideoSsrc, video_track_->GetSource()->GetVideoCapturer()));
    EXPECT_CALL(video_provider_, SetVideoSend(kVideoSsrc, true, _));
    video_rtp_sender_ = new VideoRtpSender(stream_->GetVideoTracks()[0],
                                           stream_->label(), &video_provider_);
    video_rtp_sender_->SetSsrc(kVideoSsrc);
  }

  void DestroyAudioRtpSender() {
    EXPECT_CALL(audio_provider_, SetAudioSend(kAudioSsrc, false, _, _))
        .Times(1);
    audio_rtp_sender_ = nullptr;
  }

  void DestroyVideoRtpSender() {
    EXPECT_CALL(video_provider_, SetCaptureDevice(kVideoSsrc, NULL)).Times(1);
    EXPECT_CALL(video_provider_, SetVideoSend(kVideoSsrc, false, _)).Times(1);
    video_rtp_sender_ = nullptr;
  }

  void CreateAudioRtpReceiver() {
    audio_track_ = AudioTrack::Create(
        kAudioTrackId, RemoteAudioSource::Create(kAudioSsrc, NULL));
    EXPECT_TRUE(stream_->AddTrack(audio_track_));
    EXPECT_CALL(audio_provider_, SetAudioPlayout(kAudioSsrc, true));
    audio_rtp_receiver_ = new AudioRtpReceiver(stream_->GetAudioTracks()[0],
                                               kAudioSsrc, &audio_provider_);
  }

  void CreateVideoRtpReceiver() {
    AddVideoTrack(true);
    EXPECT_CALL(video_provider_,
                SetVideoPlayout(kVideoSsrc, true,
                                video_track_->GetSource()->FrameInput()));
    video_rtp_receiver_ = new VideoRtpReceiver(stream_->GetVideoTracks()[0],
                                               kVideoSsrc, &video_provider_);
  }

  void DestroyAudioRtpReceiver() {
    EXPECT_CALL(audio_provider_, SetAudioPlayout(kAudioSsrc, false));
    audio_rtp_receiver_ = nullptr;
  }

  void DestroyVideoRtpReceiver() {
    EXPECT_CALL(video_provider_, SetVideoPlayout(kVideoSsrc, false, NULL));
    video_rtp_receiver_ = nullptr;
  }

 protected:
  MockAudioProvider audio_provider_;
  MockVideoProvider video_provider_;
  rtc::scoped_refptr<AudioRtpSender> audio_rtp_sender_;
  rtc::scoped_refptr<VideoRtpSender> video_rtp_sender_;
  rtc::scoped_refptr<AudioRtpReceiver> audio_rtp_receiver_;
  rtc::scoped_refptr<VideoRtpReceiver> video_rtp_receiver_;
  rtc::scoped_refptr<MediaStreamInterface> stream_;
  rtc::scoped_refptr<VideoTrackInterface> video_track_;
  rtc::scoped_refptr<AudioTrackInterface> audio_track_;
};

// Test that |audio_provider_| is notified when an audio track is associated
// and disassociated with an AudioRtpSender.
TEST_F(RtpSenderReceiverTest, AddAndDestroyAudioRtpSender) {
  CreateAudioRtpSender();
  DestroyAudioRtpSender();
}

// Test that |video_provider_| is notified when a video track is associated and
// disassociated with a VideoRtpSender.
TEST_F(RtpSenderReceiverTest, AddAndDestroyVideoRtpSender) {
  CreateVideoRtpSender();
  DestroyVideoRtpSender();
}

// Test that |audio_provider_| is notified when a remote audio and track is
// associated and disassociated with an AudioRtpReceiver.
TEST_F(RtpSenderReceiverTest, AddAndDestroyAudioRtpReceiver) {
  CreateAudioRtpReceiver();
  DestroyAudioRtpReceiver();
}

// Test that |video_provider_| is notified when a remote
// video track is associated and disassociated with a VideoRtpReceiver.
TEST_F(RtpSenderReceiverTest, AddAndDestroyVideoRtpReceiver) {
  CreateVideoRtpReceiver();
  DestroyVideoRtpReceiver();
}

TEST_F(RtpSenderReceiverTest, LocalAudioTrackDisable) {
  CreateAudioRtpSender();

  EXPECT_CALL(audio_provider_, SetAudioSend(kAudioSsrc, false, _, _));
  audio_track_->set_enabled(false);

  EXPECT_CALL(audio_provider_, SetAudioSend(kAudioSsrc, true, _, _));
  audio_track_->set_enabled(true);

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, RemoteAudioTrackDisable) {
  CreateAudioRtpReceiver();

  EXPECT_CALL(audio_provider_, SetAudioPlayout(kAudioSsrc, false));
  audio_track_->set_enabled(false);

  EXPECT_CALL(audio_provider_, SetAudioPlayout(kAudioSsrc, true));
  audio_track_->set_enabled(true);

  DestroyAudioRtpReceiver();
}

TEST_F(RtpSenderReceiverTest, LocalVideoTrackDisable) {
  CreateVideoRtpSender();

  EXPECT_CALL(video_provider_, SetVideoSend(kVideoSsrc, false, _));
  video_track_->set_enabled(false);

  EXPECT_CALL(video_provider_, SetVideoSend(kVideoSsrc, true, _));
  video_track_->set_enabled(true);

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, RemoteVideoTrackDisable) {
  CreateVideoRtpReceiver();

  video_track_->set_enabled(false);

  video_track_->set_enabled(true);

  DestroyVideoRtpReceiver();
}

TEST_F(RtpSenderReceiverTest, RemoteAudioTrackSetVolume) {
  CreateAudioRtpReceiver();

  double volume = 0.5;
  EXPECT_CALL(audio_provider_, SetAudioPlayoutVolume(kAudioSsrc, volume));
  audio_track_->GetSource()->SetVolume(volume);

  // Disable the audio track, this should prevent setting the volume.
  EXPECT_CALL(audio_provider_, SetAudioPlayout(kAudioSsrc, false));
  audio_track_->set_enabled(false);
  audio_track_->GetSource()->SetVolume(1.0);

  EXPECT_CALL(audio_provider_, SetAudioPlayout(kAudioSsrc, true));
  audio_track_->set_enabled(true);

  double new_volume = 0.8;
  EXPECT_CALL(audio_provider_, SetAudioPlayoutVolume(kAudioSsrc, new_volume));
  audio_track_->GetSource()->SetVolume(new_volume);

  DestroyAudioRtpReceiver();
}

// Test that provider methods aren't called without both a track and an SSRC.
TEST_F(RtpSenderReceiverTest, AudioSenderWithoutTrackAndSsrc) {
  rtc::scoped_refptr<AudioRtpSender> sender =
      new AudioRtpSender(&audio_provider_, nullptr);
  rtc::scoped_refptr<AudioTrackInterface> track =
      AudioTrack::Create(kAudioTrackId, nullptr);
  EXPECT_TRUE(sender->SetTrack(track));
  EXPECT_TRUE(sender->SetTrack(nullptr));
  sender->SetSsrc(kAudioSsrc);
  sender->SetSsrc(0);
  // Just let it get destroyed and make sure it doesn't call any methods on the
  // provider interface.
}

// Test that provider methods aren't called without both a track and an SSRC.
TEST_F(RtpSenderReceiverTest, VideoSenderWithoutTrackAndSsrc) {
  rtc::scoped_refptr<VideoRtpSender> sender =
      new VideoRtpSender(&video_provider_);
  EXPECT_TRUE(sender->SetTrack(video_track_));
  EXPECT_TRUE(sender->SetTrack(nullptr));
  sender->SetSsrc(kVideoSsrc);
  sender->SetSsrc(0);
  // Just let it get destroyed and make sure it doesn't call any methods on the
  // provider interface.
}

// Test that an audio sender calls the expected methods on the provider once
// it has a track and SSRC, when the SSRC is set first.
TEST_F(RtpSenderReceiverTest, AudioSenderEarlyWarmupSsrcThenTrack) {
  rtc::scoped_refptr<AudioRtpSender> sender =
      new AudioRtpSender(&audio_provider_, nullptr);
  rtc::scoped_refptr<AudioTrackInterface> track =
      AudioTrack::Create(kAudioTrackId, nullptr);
  sender->SetSsrc(kAudioSsrc);
  EXPECT_CALL(audio_provider_, SetAudioSend(kAudioSsrc, true, _, _));
  sender->SetTrack(track);

  // Calls expected from destructor.
  EXPECT_CALL(audio_provider_, SetAudioSend(kAudioSsrc, false, _, _)).Times(1);
}

// Test that an audio sender calls the expected methods on the provider once
// it has a track and SSRC, when the SSRC is set last.
TEST_F(RtpSenderReceiverTest, AudioSenderEarlyWarmupTrackThenSsrc) {
  rtc::scoped_refptr<AudioRtpSender> sender =
      new AudioRtpSender(&audio_provider_, nullptr);
  rtc::scoped_refptr<AudioTrackInterface> track =
      AudioTrack::Create(kAudioTrackId, nullptr);
  sender->SetTrack(track);
  EXPECT_CALL(audio_provider_, SetAudioSend(kAudioSsrc, true, _, _));
  sender->SetSsrc(kAudioSsrc);

  // Calls expected from destructor.
  EXPECT_CALL(audio_provider_, SetAudioSend(kAudioSsrc, false, _, _)).Times(1);
}

// Test that a video sender calls the expected methods on the provider once
// it has a track and SSRC, when the SSRC is set first.
TEST_F(RtpSenderReceiverTest, VideoSenderEarlyWarmupSsrcThenTrack) {
  AddVideoTrack(false);
  rtc::scoped_refptr<VideoRtpSender> sender =
      new VideoRtpSender(&video_provider_);
  sender->SetSsrc(kVideoSsrc);
  EXPECT_CALL(video_provider_,
              SetCaptureDevice(kVideoSsrc,
                               video_track_->GetSource()->GetVideoCapturer()));
  EXPECT_CALL(video_provider_, SetVideoSend(kVideoSsrc, true, _));
  sender->SetTrack(video_track_);

  // Calls expected from destructor.
  EXPECT_CALL(video_provider_, SetCaptureDevice(kVideoSsrc, nullptr)).Times(1);
  EXPECT_CALL(video_provider_, SetVideoSend(kVideoSsrc, false, _)).Times(1);
}

// Test that a video sender calls the expected methods on the provider once
// it has a track and SSRC, when the SSRC is set last.
TEST_F(RtpSenderReceiverTest, VideoSenderEarlyWarmupTrackThenSsrc) {
  AddVideoTrack(false);
  rtc::scoped_refptr<VideoRtpSender> sender =
      new VideoRtpSender(&video_provider_);
  sender->SetTrack(video_track_);
  EXPECT_CALL(video_provider_,
              SetCaptureDevice(kVideoSsrc,
                               video_track_->GetSource()->GetVideoCapturer()));
  EXPECT_CALL(video_provider_, SetVideoSend(kVideoSsrc, true, _));
  sender->SetSsrc(kVideoSsrc);

  // Calls expected from destructor.
  EXPECT_CALL(video_provider_, SetCaptureDevice(kVideoSsrc, nullptr)).Times(1);
  EXPECT_CALL(video_provider_, SetVideoSend(kVideoSsrc, false, _)).Times(1);
}

// Test that the sender is disconnected from the provider when its SSRC is
// set to 0.
TEST_F(RtpSenderReceiverTest, AudioSenderSsrcSetToZero) {
  rtc::scoped_refptr<AudioTrackInterface> track =
      AudioTrack::Create(kAudioTrackId, nullptr);
  EXPECT_CALL(audio_provider_, SetAudioSend(kAudioSsrc, true, _, _));
  rtc::scoped_refptr<AudioRtpSender> sender =
      new AudioRtpSender(track, kStreamLabel1, &audio_provider_, nullptr);
  sender->SetSsrc(kAudioSsrc);

  EXPECT_CALL(audio_provider_, SetAudioSend(kAudioSsrc, false, _, _)).Times(1);
  sender->SetSsrc(0);

  // Make sure it's SetSsrc that called methods on the provider, and not the
  // destructor.
  EXPECT_CALL(audio_provider_, SetAudioSend(_, _, _, _)).Times(0);
}

// Test that the sender is disconnected from the provider when its SSRC is
// set to 0.
TEST_F(RtpSenderReceiverTest, VideoSenderSsrcSetToZero) {
  AddVideoTrack(false);
  EXPECT_CALL(video_provider_,
              SetCaptureDevice(kVideoSsrc,
                               video_track_->GetSource()->GetVideoCapturer()));
  EXPECT_CALL(video_provider_, SetVideoSend(kVideoSsrc, true, _));
  rtc::scoped_refptr<VideoRtpSender> sender =
      new VideoRtpSender(video_track_, kStreamLabel1, &video_provider_);
  sender->SetSsrc(kVideoSsrc);

  EXPECT_CALL(video_provider_, SetCaptureDevice(kVideoSsrc, nullptr)).Times(1);
  EXPECT_CALL(video_provider_, SetVideoSend(kVideoSsrc, false, _)).Times(1);
  sender->SetSsrc(0);

  // Make sure it's SetSsrc that called methods on the provider, and not the
  // destructor.
  EXPECT_CALL(video_provider_, SetCaptureDevice(_, _)).Times(0);
  EXPECT_CALL(video_provider_, SetVideoSend(_, _, _)).Times(0);
}

TEST_F(RtpSenderReceiverTest, AudioSenderTrackSetToNull) {
  rtc::scoped_refptr<AudioTrackInterface> track =
      AudioTrack::Create(kAudioTrackId, nullptr);
  EXPECT_CALL(audio_provider_, SetAudioSend(kAudioSsrc, true, _, _));
  rtc::scoped_refptr<AudioRtpSender> sender =
      new AudioRtpSender(track, kStreamLabel1, &audio_provider_, nullptr);
  sender->SetSsrc(kAudioSsrc);

  EXPECT_CALL(audio_provider_, SetAudioSend(kAudioSsrc, false, _, _)).Times(1);
  EXPECT_TRUE(sender->SetTrack(nullptr));

  // Make sure it's SetTrack that called methods on the provider, and not the
  // destructor.
  EXPECT_CALL(audio_provider_, SetAudioSend(_, _, _, _)).Times(0);
}

TEST_F(RtpSenderReceiverTest, VideoSenderTrackSetToNull) {
  AddVideoTrack(false);
  EXPECT_CALL(video_provider_,
              SetCaptureDevice(kVideoSsrc,
                               video_track_->GetSource()->GetVideoCapturer()));
  EXPECT_CALL(video_provider_, SetVideoSend(kVideoSsrc, true, _));
  rtc::scoped_refptr<VideoRtpSender> sender =
      new VideoRtpSender(video_track_, kStreamLabel1, &video_provider_);
  sender->SetSsrc(kVideoSsrc);

  EXPECT_CALL(video_provider_, SetCaptureDevice(kVideoSsrc, nullptr)).Times(1);
  EXPECT_CALL(video_provider_, SetVideoSend(kVideoSsrc, false, _)).Times(1);
  EXPECT_TRUE(sender->SetTrack(nullptr));

  // Make sure it's SetTrack that called methods on the provider, and not the
  // destructor.
  EXPECT_CALL(video_provider_, SetCaptureDevice(_, _)).Times(0);
  EXPECT_CALL(video_provider_, SetVideoSend(_, _, _)).Times(0);
}

TEST_F(RtpSenderReceiverTest, AudioSenderSsrcChanged) {
  AddVideoTrack(false);
  rtc::scoped_refptr<AudioTrackInterface> track =
      AudioTrack::Create(kAudioTrackId, nullptr);
  EXPECT_CALL(audio_provider_, SetAudioSend(kAudioSsrc, true, _, _));
  rtc::scoped_refptr<AudioRtpSender> sender =
      new AudioRtpSender(track, kStreamLabel1, &audio_provider_, nullptr);
  sender->SetSsrc(kAudioSsrc);

  EXPECT_CALL(audio_provider_, SetAudioSend(kAudioSsrc, false, _, _)).Times(1);
  EXPECT_CALL(audio_provider_, SetAudioSend(kAudioSsrc2, true, _, _)).Times(1);
  sender->SetSsrc(kAudioSsrc2);

  // Calls expected from destructor.
  EXPECT_CALL(audio_provider_, SetAudioSend(kAudioSsrc2, false, _, _)).Times(1);
}

TEST_F(RtpSenderReceiverTest, VideoSenderSsrcChanged) {
  AddVideoTrack(false);
  EXPECT_CALL(video_provider_,
              SetCaptureDevice(kVideoSsrc,
                               video_track_->GetSource()->GetVideoCapturer()));
  EXPECT_CALL(video_provider_, SetVideoSend(kVideoSsrc, true, _));
  rtc::scoped_refptr<VideoRtpSender> sender =
      new VideoRtpSender(video_track_, kStreamLabel1, &video_provider_);
  sender->SetSsrc(kVideoSsrc);

  EXPECT_CALL(video_provider_, SetCaptureDevice(kVideoSsrc, nullptr)).Times(1);
  EXPECT_CALL(video_provider_, SetVideoSend(kVideoSsrc, false, _)).Times(1);
  EXPECT_CALL(video_provider_,
              SetCaptureDevice(kVideoSsrc2,
                               video_track_->GetSource()->GetVideoCapturer()));
  EXPECT_CALL(video_provider_, SetVideoSend(kVideoSsrc2, true, _));
  sender->SetSsrc(kVideoSsrc2);

  // Calls expected from destructor.
  EXPECT_CALL(video_provider_, SetCaptureDevice(kVideoSsrc2, nullptr)).Times(1);
  EXPECT_CALL(video_provider_, SetVideoSend(kVideoSsrc2, false, _)).Times(1);
}

}  // namespace webrtc
