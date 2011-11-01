/*
 * libjingle
 * Copyright 2011, Google Inc.
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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "talk/app/webrtc_dev/mediastreamproxy.h"
#include "talk/app/webrtc_dev/mediastreamtrackproxy.h"
#include "talk/base/refcount.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/thread.h"

static const char kStreamLabel1[] = "local_stream_1";
static const char kVideoTrackLabel[] = "dummy_video_cam_1";
static const char kAudioTrackLabel[] = "dummy_microphone_1";

using talk_base::scoped_refptr;
using ::testing::Exactly;

namespace {

class ReadyStateMessageData : public talk_base::MessageData {
 public:
  ReadyStateMessageData(
      webrtc::MediaStreamInterface* stream,
      webrtc::MediaStreamInterface::ReadyState new_state)
      : stream_(stream),
        ready_state_(new_state) {
  }

  scoped_refptr<webrtc::MediaStreamInterface> stream_;
  webrtc::MediaStreamInterface::ReadyState ready_state_;
};

class TrackStateMessageData : public talk_base::MessageData {
 public:
  TrackStateMessageData(
      webrtc::MediaStreamTrackInterface* track,
      webrtc::MediaStreamTrackInterface::TrackState state)
      : track_(track),
        state_(state) {
  }

  scoped_refptr<webrtc::MediaStreamTrackInterface> track_;
  webrtc::MediaStreamTrackInterface::TrackState state_;
};

}  // namespace anonymous

namespace webrtc {

// Helper class to test Observer.
class MockObserver : public ObserverInterface {
 public:
  explicit MockObserver(talk_base::Thread* signaling_thread)
      : signaling_thread_(signaling_thread) {
  }

  MOCK_METHOD0(DoOnChanged, void());
  virtual void OnChanged() {
    ASSERT_TRUE(talk_base::Thread::Current() == signaling_thread_);
    DoOnChanged();
  }
 private:
  talk_base::Thread* signaling_thread_;
};

class MockMediaStream: public LocalMediaStreamInterface {
 public:
  MockMediaStream(const std::string& label, talk_base::Thread* signaling_thread)
      : stream_impl_(MediaStream::Create(label)),
        signaling_thread_(signaling_thread) {
  }
  virtual void RegisterObserver(webrtc::ObserverInterface* observer) {
    EXPECT_EQ(talk_base::Thread::Current(), signaling_thread_);
    stream_impl_->RegisterObserver(observer);
  }
  virtual void UnregisterObserver(webrtc::ObserverInterface* observer) {
    EXPECT_EQ(talk_base::Thread::Current(), signaling_thread_);
    stream_impl_->UnregisterObserver(observer);
  }
  virtual std::string label() const {
    EXPECT_EQ(talk_base::Thread::Current(), signaling_thread_);
    return stream_impl_->label();
  }
  virtual AudioTracks* audio_tracks() {
    EXPECT_EQ(talk_base::Thread::Current(), signaling_thread_);
    return stream_impl_->audio_tracks();
  }
  virtual VideoTracks* video_tracks() {
    EXPECT_EQ(talk_base::Thread::Current(), signaling_thread_);
    return stream_impl_->video_tracks();
  }
  virtual ReadyState ready_state() {
    EXPECT_EQ(talk_base::Thread::Current(), signaling_thread_);
    return stream_impl_->ready_state();
  }
  virtual void set_ready_state(ReadyState state) {
    EXPECT_EQ(talk_base::Thread::Current(), signaling_thread_);
    return stream_impl_->set_ready_state(state);
  }
  virtual bool AddTrack(AudioTrackInterface* audio_track) {
    EXPECT_EQ(talk_base::Thread::Current(), signaling_thread_);
    return stream_impl_->AddTrack(audio_track);
  }
  virtual bool AddTrack(VideoTrackInterface* video_track) {
    EXPECT_EQ(talk_base::Thread::Current(), signaling_thread_);
    return stream_impl_->AddTrack(video_track);
  }

 private:
  scoped_refptr<MediaStream> stream_impl_;
  talk_base::Thread* signaling_thread_;
};

template <class T>
class MockMediaStreamTrack: public T {
 public:
  MockMediaStreamTrack(T* implementation,
                       talk_base::Thread* signaling_thread)
      : track_impl_(implementation),
        signaling_thread_(signaling_thread) {
  }
  virtual void RegisterObserver(webrtc::ObserverInterface* observer) {
    EXPECT_EQ(talk_base::Thread::Current(), signaling_thread_);
    track_impl_->RegisterObserver(observer);
  }
  virtual void UnregisterObserver(webrtc::ObserverInterface* observer) {
    EXPECT_EQ(talk_base::Thread::Current(), signaling_thread_);
    track_impl_->UnregisterObserver(observer);
  }
  virtual std::string kind() const {
    EXPECT_EQ(talk_base::Thread::Current(), signaling_thread_);
    return track_impl_->kind();
  }
  virtual std::string label() const {
    EXPECT_EQ(talk_base::Thread::Current(), signaling_thread_);
    return track_impl_->label();
  }
  virtual bool enabled() const {
    EXPECT_EQ(talk_base::Thread::Current(), signaling_thread_);
    return track_impl_->enabled();
  }
  virtual MediaStreamTrackInterface::TrackState state() const {
    EXPECT_EQ(talk_base::Thread::Current(), signaling_thread_);
    return track_impl_->state();
  }
  virtual bool set_enabled(bool enabled) {
    EXPECT_EQ(talk_base::Thread::Current(), signaling_thread_);
    return track_impl_->set_enabled(enabled);
  }
  virtual bool set_state(webrtc::MediaStreamTrackInterface::TrackState state) {
    EXPECT_EQ(talk_base::Thread::Current(), signaling_thread_);
    return track_impl_->set_state(state);
  }

 protected:
  scoped_refptr<T> track_impl_;
  talk_base::Thread* signaling_thread_;
};

class MockLocalVideoTrack
    : public MockMediaStreamTrack<LocalVideoTrackInterface> {
 public:
    MockLocalVideoTrack(LocalVideoTrackInterface* implementation,
                        talk_base::Thread* signaling_thread)
        : MockMediaStreamTrack<LocalVideoTrackInterface>(implementation,
                                                         signaling_thread) {
    }
  virtual void SetRenderer(webrtc::VideoRendererWrapperInterface* renderer) {
    EXPECT_EQ(talk_base::Thread::Current(), signaling_thread_);
    track_impl_->SetRenderer(renderer);
  }
  virtual VideoRendererWrapperInterface* GetRenderer() {
    EXPECT_EQ(talk_base::Thread::Current(), signaling_thread_);
    return track_impl_->GetRenderer();
  }
  virtual VideoCaptureModule* GetVideoCapture() {
    EXPECT_EQ(talk_base::Thread::Current(), signaling_thread_);
    return track_impl_->GetVideoCapture();
  }
};

class MockLocalAudioTrack
    : public MockMediaStreamTrack<LocalAudioTrackInterface> {
 public:
  MockLocalAudioTrack(LocalAudioTrackInterface* implementation,
                      talk_base::Thread* signaling_thread)
    : MockMediaStreamTrack<LocalAudioTrackInterface>(implementation,
                                                     signaling_thread) {
  }

  virtual AudioDeviceModule* GetAudioDevice() {
    EXPECT_EQ(talk_base::Thread::Current(), signaling_thread_);
    return track_impl_->GetAudioDevice();
  }
};

class MediaStreamTest: public testing::Test,
                       public talk_base::MessageHandler {
 protected:
  virtual void SetUp() {
    signaling_thread_ .reset(new talk_base::Thread());
    ASSERT_TRUE(signaling_thread_->Start());

    std::string label(kStreamLabel1);
    // Create a stream proxy object that uses our mocked
    // version of a LocalMediaStream.
    scoped_refptr<MockMediaStream> mock_stream(
        new talk_base::RefCountedObject<MockMediaStream>(label,
                                                 signaling_thread_.get()));
    stream_ = MediaStreamProxy::Create(label, signaling_thread_.get(),
                                       mock_stream);
    ASSERT_TRUE(stream_.get());
    EXPECT_EQ(label, stream_->label());
    EXPECT_EQ(MediaStreamInterface::kInitializing, stream_->ready_state());

    // Create a video track proxy object that uses our mocked
    // version of a LocalVideoTrack
    scoped_refptr<VideoTrack> video_track_impl(
        VideoTrack::CreateLocal(kVideoTrackLabel, NULL));
    scoped_refptr<MockLocalVideoTrack> mock_videotrack(
        new talk_base::RefCountedObject<MockLocalVideoTrack>(video_track_impl,
                                                     signaling_thread_.get()));
    video_track_ = VideoTrackProxy::CreateLocal(mock_videotrack,
                                                signaling_thread_.get());

    ASSERT_TRUE(video_track_.get());
    EXPECT_EQ(MediaStreamTrackInterface::kInitializing, video_track_->state());

    // Create an audio track proxy object that uses our mocked
    // version of a LocalAudioTrack
    scoped_refptr<AudioTrack> audio_track_impl(
        AudioTrack::CreateLocal(kAudioTrackLabel, NULL));
    scoped_refptr<MockLocalAudioTrack> mock_audiotrack(
        new talk_base::RefCountedObject<MockLocalAudioTrack>(audio_track_impl,
                                                     signaling_thread_.get()));
    audio_track_ = AudioTrackProxy::CreateLocal(mock_audiotrack,
                                                signaling_thread_.get());

    ASSERT_TRUE(audio_track_.get());
    EXPECT_EQ(MediaStreamTrackInterface::kInitializing, audio_track_->state());
  }

  enum {
    MSG_SET_READYSTATE,
    MSG_SET_TRACKSTATE,
  };

  // Set the ready state on the signaling thread.
  // State can only be changed on the signaling thread.
  void SetReadyState(MediaStreamInterface* stream,
                     MediaStreamInterface::ReadyState new_state) {
    ReadyStateMessageData state(stream, new_state);
    signaling_thread_->Send(this, MSG_SET_READYSTATE, &state);
  }

  // Set the track state on the signaling thread.
  // State can only be changed on the signaling thread.
  void SetTrackState(MediaStreamTrackInterface* track,
                     MediaStreamTrackInterface::TrackState new_state) {
    TrackStateMessageData state(track, new_state);
    signaling_thread_->Send(this, MSG_SET_TRACKSTATE, &state);
  }

  talk_base::scoped_ptr<talk_base::Thread> signaling_thread_;
  scoped_refptr<LocalMediaStreamInterface> stream_;
  scoped_refptr<LocalVideoTrackInterface> video_track_;
  scoped_refptr<LocalAudioTrackInterface> audio_track_;

 private:
  // Implements talk_base::MessageHandler.
  virtual void OnMessage(talk_base::Message* msg) {
    switch (msg->message_id) {
      case MSG_SET_READYSTATE: {
        ReadyStateMessageData* state =
            static_cast<ReadyStateMessageData*>(msg->pdata);
        state->stream_->set_ready_state(state->ready_state_);
        break;
      }
      case MSG_SET_TRACKSTATE: {
        TrackStateMessageData* state =
            static_cast<TrackStateMessageData*>(msg->pdata);
        state->track_->set_state(state->state_);
        break;
      }
      default:
        break;
    }
  }
};

TEST_F(MediaStreamTest, CreateLocalStream) {
  EXPECT_TRUE(stream_->AddTrack(video_track_));
  EXPECT_TRUE(stream_->AddTrack(audio_track_));

  ASSERT_EQ(1u, stream_->video_tracks()->count());
  ASSERT_EQ(1u, stream_->audio_tracks()->count());

  // Verify the video track.
  scoped_refptr<webrtc::MediaStreamTrackInterface> track(
      stream_->video_tracks()->at(0));
  EXPECT_EQ(0, track->label().compare(kVideoTrackLabel));
  EXPECT_TRUE(track->enabled());

  // Verify the audio track.
  track = stream_->audio_tracks()->at(0);
  EXPECT_EQ(0, track->label().compare(kAudioTrackLabel));
  EXPECT_TRUE(track->enabled());
}

TEST_F(MediaStreamTest, ChangeStreamState) {
  MockObserver observer(signaling_thread_.get());
  stream_->RegisterObserver(&observer);

  EXPECT_CALL(observer, DoOnChanged())
      .Times(Exactly(1));
  SetReadyState(stream_, MediaStreamInterface::kLive);

  EXPECT_EQ(MediaStreamInterface::kLive, stream_->ready_state());
  // It should not be possible to add
  // streams when the state has changed to live.
  EXPECT_FALSE(stream_->AddTrack(audio_track_));
  EXPECT_EQ(0u, stream_->audio_tracks()->count());
}

TEST_F(MediaStreamTest, ChangeVideoTrack) {
  MockObserver observer(signaling_thread_.get());
  video_track_->RegisterObserver(&observer);

  EXPECT_CALL(observer, DoOnChanged())
      .Times(Exactly(1));
  video_track_->set_enabled(false);
  EXPECT_FALSE(video_track_->state());

  EXPECT_CALL(observer, DoOnChanged())
      .Times(Exactly(1));
  SetTrackState(video_track_, MediaStreamTrackInterface::kLive);
  EXPECT_EQ(MediaStreamTrackInterface::kLive, video_track_->state());

  EXPECT_CALL(observer, DoOnChanged())
      .Times(Exactly(1));
  scoped_refptr<VideoRendererWrapperInterface> renderer(
      CreateVideoRenderer(NULL));
  video_track_->SetRenderer(renderer.get());
  EXPECT_TRUE(renderer.get() == video_track_->GetRenderer());
}

TEST_F(MediaStreamTest, ChangeAudioTrack) {
  MockObserver observer(signaling_thread_.get());
  audio_track_->RegisterObserver(&observer);

  EXPECT_CALL(observer, DoOnChanged())
      .Times(Exactly(1));
  audio_track_->set_enabled(false);
  EXPECT_FALSE(audio_track_->enabled());

  EXPECT_CALL(observer, DoOnChanged())
      .Times(Exactly(1));
  SetTrackState(audio_track_, MediaStreamTrackInterface::kLive);
  EXPECT_EQ(MediaStreamTrackInterface::kLive, audio_track_->state());
}

}  // namespace webrtc
