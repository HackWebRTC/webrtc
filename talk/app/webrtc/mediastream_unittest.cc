/*
 * libjingle
 * Copyright 2011 Google Inc.
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

#include "talk/app/webrtc/audiotrack.h"
#include "talk/app/webrtc/mediastream.h"
#include "talk/app/webrtc/videotrack.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/refcount.h"
#include "webrtc/base/scoped_ptr.h"

static const char kStreamLabel1[] = "local_stream_1";
static const char kVideoTrackId[] = "dummy_video_cam_1";
static const char kAudioTrackId[] = "dummy_microphone_1";

using rtc::scoped_refptr;
using ::testing::Exactly;

namespace webrtc {

// Helper class to test Observer.
class MockObserver : public ObserverInterface {
 public:
  explicit MockObserver(NotifierInterface* notifier) : notifier_(notifier) {
    notifier_->RegisterObserver(this);
  }

  ~MockObserver() { Unregister(); }

  void Unregister() {
    if (notifier_) {
      notifier_->UnregisterObserver(this);
      notifier_ = nullptr;
    }
  }

  MOCK_METHOD0(OnChanged, void());

 private:
  NotifierInterface* notifier_;
};

class MediaStreamTest: public testing::Test {
 protected:
  virtual void SetUp() {
    stream_ = MediaStream::Create(kStreamLabel1);
    ASSERT_TRUE(stream_.get() != NULL);

    video_track_ = VideoTrack::Create(kVideoTrackId, NULL);
    ASSERT_TRUE(video_track_.get() != NULL);
    EXPECT_EQ(MediaStreamTrackInterface::kInitializing, video_track_->state());

    audio_track_ = AudioTrack::Create(kAudioTrackId, NULL);

    ASSERT_TRUE(audio_track_.get() != NULL);
    EXPECT_EQ(MediaStreamTrackInterface::kInitializing, audio_track_->state());

    EXPECT_TRUE(stream_->AddTrack(video_track_));
    EXPECT_FALSE(stream_->AddTrack(video_track_));
    EXPECT_TRUE(stream_->AddTrack(audio_track_));
    EXPECT_FALSE(stream_->AddTrack(audio_track_));
  }

  void ChangeTrack(MediaStreamTrackInterface* track) {
    MockObserver observer(track);

    EXPECT_CALL(observer, OnChanged())
        .Times(Exactly(1));
    track->set_enabled(false);
    EXPECT_FALSE(track->enabled());

    EXPECT_CALL(observer, OnChanged())
        .Times(Exactly(1));
    track->set_state(MediaStreamTrackInterface::kLive);
    EXPECT_EQ(MediaStreamTrackInterface::kLive, track->state());
  }

  scoped_refptr<MediaStreamInterface> stream_;
  scoped_refptr<AudioTrackInterface> audio_track_;
  scoped_refptr<VideoTrackInterface> video_track_;
};

TEST_F(MediaStreamTest, GetTrackInfo) {
  ASSERT_EQ(1u, stream_->GetVideoTracks().size());
  ASSERT_EQ(1u, stream_->GetAudioTracks().size());

  // Verify the video track.
  scoped_refptr<webrtc::MediaStreamTrackInterface> video_track(
      stream_->GetVideoTracks()[0]);
  EXPECT_EQ(0, video_track->id().compare(kVideoTrackId));
  EXPECT_TRUE(video_track->enabled());

  ASSERT_EQ(1u, stream_->GetVideoTracks().size());
  EXPECT_TRUE(stream_->GetVideoTracks()[0].get() == video_track.get());
  EXPECT_TRUE(stream_->FindVideoTrack(video_track->id()).get()
              == video_track.get());
  video_track = stream_->GetVideoTracks()[0];
  EXPECT_EQ(0, video_track->id().compare(kVideoTrackId));
  EXPECT_TRUE(video_track->enabled());

  // Verify the audio track.
  scoped_refptr<webrtc::MediaStreamTrackInterface> audio_track(
      stream_->GetAudioTracks()[0]);
  EXPECT_EQ(0, audio_track->id().compare(kAudioTrackId));
  EXPECT_TRUE(audio_track->enabled());
  ASSERT_EQ(1u, stream_->GetAudioTracks().size());
  EXPECT_TRUE(stream_->GetAudioTracks()[0].get() == audio_track.get());
  EXPECT_TRUE(stream_->FindAudioTrack(audio_track->id()).get()
              == audio_track.get());
  audio_track = stream_->GetAudioTracks()[0];
  EXPECT_EQ(0, audio_track->id().compare(kAudioTrackId));
  EXPECT_TRUE(audio_track->enabled());
}

TEST_F(MediaStreamTest, RemoveTrack) {
  MockObserver observer(stream_);

  EXPECT_CALL(observer, OnChanged())
      .Times(Exactly(2));

  EXPECT_TRUE(stream_->RemoveTrack(audio_track_));
  EXPECT_FALSE(stream_->RemoveTrack(audio_track_));
  EXPECT_EQ(0u, stream_->GetAudioTracks().size());
  EXPECT_EQ(0u, stream_->GetAudioTracks().size());

  EXPECT_TRUE(stream_->RemoveTrack(video_track_));
  EXPECT_FALSE(stream_->RemoveTrack(video_track_));

  EXPECT_EQ(0u, stream_->GetVideoTracks().size());
  EXPECT_EQ(0u, stream_->GetVideoTracks().size());

  EXPECT_FALSE(stream_->RemoveTrack(static_cast<AudioTrackInterface*>(NULL)));
  EXPECT_FALSE(stream_->RemoveTrack(static_cast<VideoTrackInterface*>(NULL)));
}

TEST_F(MediaStreamTest, ChangeVideoTrack) {
  scoped_refptr<webrtc::VideoTrackInterface> video_track(
      stream_->GetVideoTracks()[0]);
  ChangeTrack(video_track.get());
}

TEST_F(MediaStreamTest, ChangeAudioTrack) {
  scoped_refptr<webrtc::AudioTrackInterface> audio_track(
      stream_->GetAudioTracks()[0]);
  ChangeTrack(audio_track.get());
}

}  // namespace webrtc
