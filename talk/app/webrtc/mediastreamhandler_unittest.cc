/*
 * libjingle
 * Copyright 2012, Google Inc.
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

#include "talk/app/webrtc/mediastreamhandler.h"

#include <string>

#include "talk/app/webrtc/audiotrack.h"
#include "talk/app/webrtc/mediastream.h"
#include "talk/app/webrtc/streamcollection.h"
#include "talk/app/webrtc/videosource.h"
#include "talk/app/webrtc/videotrack.h"
#include "talk/base/gunit.h"
#include "talk/media/base/fakevideocapturer.h"
#include "talk/media/base/mediachannel.h"
#include "testing/base/public/gmock.h"

using ::testing::_;
using ::testing::Exactly;

static const char kStreamLabel1[] = "local_stream_1";
static const char kVideoTrackId[] = "video_1";
static const char kAudioTrackId[] = "audio_1";
static const uint32 kVideoSsrc = 98;
static const uint32 kAudioSsrc = 99;

namespace webrtc {

// Helper class to test MediaStreamHandler.
class MockAudioProvider : public AudioProviderInterface {
 public:
  virtual ~MockAudioProvider() {}
  MOCK_METHOD3(SetAudioPlayout, void(uint32 ssrc, bool enable,
                                     cricket::AudioRenderer* renderer));
  MOCK_METHOD4(SetAudioSend, void(uint32 ssrc, bool enable,
                                  const cricket::AudioOptions& options,
                                  cricket::AudioRenderer* renderer));
};

// Helper class to test MediaStreamHandler.
class MockVideoProvider : public VideoProviderInterface {
 public:
  virtual ~MockVideoProvider() {}
  MOCK_METHOD2(SetCaptureDevice, bool(uint32 ssrc,
                                      cricket::VideoCapturer* camera));
  MOCK_METHOD3(SetVideoPlayout, void(uint32 ssrc,
                                     bool enable,
                                     cricket::VideoRenderer* renderer));
  MOCK_METHOD3(SetVideoSend, void(uint32 ssrc, bool enable,
                                  const cricket::VideoOptions* options));
};

class FakeVideoSource : public Notifier<VideoSourceInterface> {
 public:
  static talk_base::scoped_refptr<FakeVideoSource> Create() {
    return new talk_base::RefCountedObject<FakeVideoSource>();
  }
  virtual cricket::VideoCapturer* GetVideoCapturer() {
    return &fake_capturer_;
  }
  virtual void AddSink(cricket::VideoRenderer* output) {}
  virtual void RemoveSink(cricket::VideoRenderer* output) {}
  virtual SourceState state() const { return state_; }
  virtual const cricket::VideoOptions* options() const { return &options_; }
  virtual cricket::VideoRenderer* FrameInput() { return NULL; }

 protected:
  FakeVideoSource() : state_(kLive) {}
  ~FakeVideoSource() {}

 private:
  cricket::FakeVideoCapturer fake_capturer_;
  SourceState state_;
  cricket::VideoOptions options_;
};

class MediaStreamHandlerTest : public testing::Test {
 public:
  MediaStreamHandlerTest()
      : handlers_(&audio_provider_, &video_provider_) {
  }

  virtual void SetUp() {
    stream_ = MediaStream::Create(kStreamLabel1);
    talk_base::scoped_refptr<VideoSourceInterface> source(
        FakeVideoSource::Create());
    video_track_ = VideoTrack::Create(kVideoTrackId, source);
    EXPECT_TRUE(stream_->AddTrack(video_track_));
    audio_track_ = AudioTrack::Create(kAudioTrackId,
                                           NULL);
    EXPECT_TRUE(stream_->AddTrack(audio_track_));
  }

  void AddLocalAudioTrack() {
    EXPECT_CALL(audio_provider_, SetAudioSend(kAudioSsrc, true, _, _));
    handlers_.AddLocalAudioTrack(stream_, stream_->GetAudioTracks()[0],
                                 kAudioSsrc);
  }

  void AddLocalVideoTrack() {
    EXPECT_CALL(video_provider_, SetCaptureDevice(
        kVideoSsrc, video_track_->GetSource()->GetVideoCapturer()));
    EXPECT_CALL(video_provider_, SetVideoSend(kVideoSsrc, true, _));
    handlers_.AddLocalVideoTrack(stream_, stream_->GetVideoTracks()[0],
                                 kVideoSsrc);
  }

  void RemoveLocalAudioTrack() {
    EXPECT_CALL(audio_provider_, SetAudioSend(kAudioSsrc, false, _, _))
        .Times(1);
    handlers_.RemoveLocalTrack(stream_, audio_track_);
  }

  void RemoveLocalVideoTrack() {
    EXPECT_CALL(video_provider_, SetCaptureDevice(kVideoSsrc, NULL))
        .Times(1);
    EXPECT_CALL(video_provider_, SetVideoSend(kVideoSsrc, false, _))
        .Times(1);
    handlers_.RemoveLocalTrack(stream_, video_track_);
  }

  void AddRemoteAudioTrack() {
    EXPECT_CALL(audio_provider_, SetAudioPlayout(kAudioSsrc, true, _));
    handlers_.AddRemoteAudioTrack(stream_, stream_->GetAudioTracks()[0],
                                  kAudioSsrc);
  }

  void AddRemoteVideoTrack() {
    EXPECT_CALL(video_provider_, SetVideoPlayout(
        kVideoSsrc, true, video_track_->GetSource()->FrameInput()));
    handlers_.AddRemoteVideoTrack(stream_, stream_->GetVideoTracks()[0],
                                  kVideoSsrc);
  }

  void RemoveRemoteAudioTrack() {
    EXPECT_CALL(audio_provider_, SetAudioPlayout(kAudioSsrc, false, _));
    handlers_.RemoveRemoteTrack(stream_, stream_->GetAudioTracks()[0]);
  }

  void RemoveRemoteVideoTrack() {
    EXPECT_CALL(video_provider_, SetVideoPlayout(kVideoSsrc, false, NULL));
    handlers_.RemoveRemoteTrack(stream_, stream_->GetVideoTracks()[0]);
  }

 protected:
  MockAudioProvider audio_provider_;
  MockVideoProvider video_provider_;
  MediaStreamHandlerContainer handlers_;
  talk_base::scoped_refptr<MediaStreamInterface> stream_;
  talk_base::scoped_refptr<VideoTrackInterface> video_track_;
  talk_base::scoped_refptr<AudioTrackInterface> audio_track_;
};

// Test that |audio_provider_| is notified when an audio track is associated
// and disassociated with a MediaStreamHandler.
TEST_F(MediaStreamHandlerTest, AddAndRemoveLocalAudioTrack) {
  AddLocalAudioTrack();
  RemoveLocalAudioTrack();

  handlers_.RemoveLocalStream(stream_);
}

// Test that |video_provider_| is notified when a video track is associated and
// disassociated with a MediaStreamHandler.
TEST_F(MediaStreamHandlerTest, AddAndRemoveLocalVideoTrack) {
  AddLocalVideoTrack();
  RemoveLocalVideoTrack();

  handlers_.RemoveLocalStream(stream_);
}

// Test that |video_provider_| and |audio_provider_| is notified when an audio
// and video track is disassociated with a MediaStreamHandler by calling
// RemoveLocalStream.
TEST_F(MediaStreamHandlerTest, RemoveLocalStream) {
  AddLocalAudioTrack();
  AddLocalVideoTrack();

  EXPECT_CALL(video_provider_, SetCaptureDevice(kVideoSsrc, NULL))
      .Times(1);
  EXPECT_CALL(video_provider_, SetVideoSend(kVideoSsrc, false, _))
      .Times(1);
  EXPECT_CALL(audio_provider_, SetAudioSend(kAudioSsrc, false, _, _))
      .Times(1);
  handlers_.RemoveLocalStream(stream_);
}


// Test that |audio_provider_| is notified when a remote audio and track is
// associated and disassociated with a MediaStreamHandler.
TEST_F(MediaStreamHandlerTest, AddAndRemoveRemoteAudioTrack) {
  AddRemoteAudioTrack();
  RemoveRemoteAudioTrack();

  handlers_.RemoveRemoteStream(stream_);
}

// Test that |video_provider_| is notified when a remote
// video track is associated and disassociated with a MediaStreamHandler.
TEST_F(MediaStreamHandlerTest, AddAndRemoveRemoteVideoTrack) {
  AddRemoteVideoTrack();
  RemoveRemoteVideoTrack();

  handlers_.RemoveRemoteStream(stream_);
}

// Test that |audio_provider_| and |video_provider_| is notified when an audio
// and video track is disassociated with a MediaStreamHandler by calling
// RemoveRemoveStream.
TEST_F(MediaStreamHandlerTest, RemoveRemoteStream) {
  AddRemoteAudioTrack();
  AddRemoteVideoTrack();

  EXPECT_CALL(video_provider_, SetVideoPlayout(kVideoSsrc, false, NULL))
      .Times(1);
  EXPECT_CALL(audio_provider_, SetAudioPlayout(kAudioSsrc, false, _))
      .Times(1);
  handlers_.RemoveRemoteStream(stream_);
}

TEST_F(MediaStreamHandlerTest, LocalAudioTrackDisable) {
  AddLocalAudioTrack();

  EXPECT_CALL(audio_provider_, SetAudioSend(kAudioSsrc, false, _, _));
  audio_track_->set_enabled(false);

  EXPECT_CALL(audio_provider_, SetAudioSend(kAudioSsrc, true, _, _));
  audio_track_->set_enabled(true);

  RemoveLocalAudioTrack();
  handlers_.TearDown();
}

TEST_F(MediaStreamHandlerTest, RemoteAudioTrackDisable) {
  AddRemoteAudioTrack();

  EXPECT_CALL(audio_provider_, SetAudioPlayout(kAudioSsrc, false, _));
  audio_track_->set_enabled(false);

  EXPECT_CALL(audio_provider_, SetAudioPlayout(kAudioSsrc, true, _));
  audio_track_->set_enabled(true);

  RemoveRemoteAudioTrack();
  handlers_.TearDown();
}

TEST_F(MediaStreamHandlerTest, LocalVideoTrackDisable) {
  AddLocalVideoTrack();

  EXPECT_CALL(video_provider_, SetVideoSend(kVideoSsrc, false, _));
  video_track_->set_enabled(false);

  EXPECT_CALL(video_provider_, SetVideoSend(kVideoSsrc, true, _));
  video_track_->set_enabled(true);

  RemoveLocalVideoTrack();
  handlers_.TearDown();
}

TEST_F(MediaStreamHandlerTest, RemoteVideoTrackDisable) {
  AddRemoteVideoTrack();

  video_track_->set_enabled(false);

  video_track_->set_enabled(true);

  RemoveRemoteVideoTrack();
  handlers_.TearDown();
}

}  // namespace webrtc
