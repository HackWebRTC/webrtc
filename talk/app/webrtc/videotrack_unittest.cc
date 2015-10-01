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

#include "talk/app/webrtc/remotevideocapturer.h"
#include "talk/app/webrtc/test/fakevideotrackrenderer.h"
#include "talk/app/webrtc/videosource.h"
#include "talk/app/webrtc/videotrack.h"
#include "talk/media/base/fakemediaengine.h"
#include "talk/media/webrtc/webrtcvideoframe.h"
#include "talk/session/media/channelmanager.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/scoped_ptr.h"

using webrtc::FakeVideoTrackRenderer;
using webrtc::VideoSource;
using webrtc::VideoTrack;
using webrtc::VideoTrackInterface;

namespace {

class WebRtcVideoTestFrame : public cricket::WebRtcVideoFrame {
 public:
  using cricket::WebRtcVideoFrame::SetRotation;
};

}  // namespace

class VideoTrackTest : public testing::Test {
 public:
  VideoTrackTest() {
    static const char kVideoTrackId[] = "track_id";

    channel_manager_.reset(new cricket::ChannelManager(
        new cricket::FakeMediaEngine(), rtc::Thread::Current()));
    EXPECT_TRUE(channel_manager_->Init());
    video_track_ = VideoTrack::Create(
        kVideoTrackId,
        VideoSource::Create(channel_manager_.get(),
                            new webrtc::RemoteVideoCapturer(), NULL));
  }

 protected:
  rtc::scoped_ptr<cricket::ChannelManager> channel_manager_;
  rtc::scoped_refptr<VideoTrackInterface> video_track_;
};

// Test adding renderers to a video track and render to them by providing
// frames to the source.
TEST_F(VideoTrackTest, RenderVideo) {
  // FakeVideoTrackRenderer register itself to |video_track_|
  rtc::scoped_ptr<FakeVideoTrackRenderer> renderer_1(
      new FakeVideoTrackRenderer(video_track_.get()));

  cricket::VideoRenderer* renderer_input =
      video_track_->GetSource()->FrameInput();
  ASSERT_FALSE(renderer_input == NULL);

  cricket::WebRtcVideoFrame frame;
  frame.InitToBlack(123, 123, 1, 1, 0);
  renderer_input->RenderFrame(&frame);
  EXPECT_EQ(1, renderer_1->num_rendered_frames());

  EXPECT_EQ(123, renderer_1->width());
  EXPECT_EQ(123, renderer_1->height());

  // FakeVideoTrackRenderer register itself to |video_track_|
  rtc::scoped_ptr<FakeVideoTrackRenderer> renderer_2(
      new FakeVideoTrackRenderer(video_track_.get()));

  renderer_input->RenderFrame(&frame);

  EXPECT_EQ(123, renderer_1->width());
  EXPECT_EQ(123, renderer_1->height());
  EXPECT_EQ(123, renderer_2->width());
  EXPECT_EQ(123, renderer_2->height());

  EXPECT_EQ(2, renderer_1->num_rendered_frames());
  EXPECT_EQ(1, renderer_2->num_rendered_frames());

  video_track_->RemoveRenderer(renderer_1.get());
  renderer_input->RenderFrame(&frame);

  EXPECT_EQ(2, renderer_1->num_rendered_frames());
  EXPECT_EQ(2, renderer_2->num_rendered_frames());
}
