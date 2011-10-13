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

#include "gtest/gtest.h"
#include "talk/app/webrtc_dev/mediastreamimpl.h"

static const char kStreamLabel1[] = "local_stream_1";
static const char kVideoDeviceName[] = "dummy_video_cam_1";

namespace webrtc {

// Helper class to test the Observer.
class TestObserver : public Observer {
 public:
  TestObserver() : changed_(0) {}
  void OnChanged() {
    ++changed_;
  }
  int NoChanged() {
    return changed_;
  }

 protected:
  int changed_;
};

TEST(LocalStreamTest, Create) {
  // Create a local stream.
  std::string label(kStreamLabel1);
  scoped_refptr<LocalMediaStream> stream(MediaStreamImpl::Create(label));

  EXPECT_EQ(stream->label().compare(label), 0);
  //  Check state.
  EXPECT_EQ(stream->ready_state(), MediaStream::kInitializing);

  // Create a local Video track.
  {
    TestObserver tracklist_observer;

    scoped_refptr<LocalVideoTrack> video_track(CreateLocalVideoTrack(
        kVideoDeviceName, NULL));

    // Add an observer to the track list.
    scoped_refptr<MediaStreamTrackList> track_list(stream->tracks());
    stream->tracks()->RegisterObserver(&tracklist_observer);

    // Add the track to the local stream.
    EXPECT_TRUE(stream->AddTrack(video_track));

    // Verify that the track list observer have been notified
    // that the track have been added.
    EXPECT_EQ(tracklist_observer.NoChanged(), 1);
  }

  EXPECT_EQ(stream->tracks()->count(), 1u);

  // Verify the track.
  scoped_refptr<webrtc::MediaStreamTrack> track(stream->tracks()->at(0));
  EXPECT_EQ(track->kind().compare(kVideoTrackKind), 0);
  EXPECT_EQ(track->label().compare(kVideoDeviceName), 0);
  EXPECT_TRUE(track->enabled());

  // Verify the Track observer.
  TestObserver observer1;
  TestObserver observer2;
  track->RegisterObserver(&observer1);
  track->RegisterObserver(&observer2);
  track->set_enabled(false);
  EXPECT_EQ(observer1.NoChanged(), 1);
  EXPECT_EQ(observer2.NoChanged(), 1);
  EXPECT_FALSE(track->enabled());
}

}  // namespace webrtc
