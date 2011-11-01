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
#include "talk/app/webrtc_dev/mediastreamimpl.h"
#include "talk/app/webrtc_dev/videotrackimpl.h"
#include "talk/app/webrtc_dev/mediastreamhandler.h"
#include "talk/app/webrtc_dev/streamcollectionimpl.h"
#include "talk/base/thread.h"

using ::testing::Exactly;

static const char kStreamLabel1[] = "local_stream_1";
static const char kVideoDeviceName[] = "dummy_video_cam_1";

namespace webrtc {

// Helper class to test MediaStreamHandler.
class MockMediaProvier : public MediaProviderInterface {
 public:
  MOCK_METHOD1(SetCaptureDevice, void(const std::string& name));
  MOCK_METHOD1(SetLocalRenderer, void(const std::string& name));
  MOCK_METHOD1(SetRemoteRenderer, void(const std::string& name));

  virtual void SetCaptureDevice(const std::string& name,
                                VideoCaptureModule* camera) {
    SetCaptureDevice(name);
  }
  virtual void SetLocalRenderer(const std::string& name,
                                cricket::VideoRenderer* renderer) {
    SetLocalRenderer(name);
  }

  virtual void SetRemoteRenderer(const std::string& name,
                                 cricket::VideoRenderer* renderer) {
    SetRemoteRenderer(name);
  }
  ~MockMediaProvier() {}
};

TEST(MediaStreamHandlerTest, LocalStreams) {
  // Create a local stream.
  std::string label(kStreamLabel1);
  talk_base::scoped_refptr<LocalMediaStreamInterface> stream(
      MediaStream::Create(label));
  talk_base::scoped_refptr<LocalVideoTrackInterface>
      video_track(VideoTrack::CreateLocal(kVideoDeviceName, NULL));
  EXPECT_TRUE(stream->AddTrack(video_track));
  talk_base::scoped_refptr<VideoRendererWrapperInterface> renderer(
      CreateVideoRenderer(NULL));
  video_track->SetRenderer(renderer);

  MockMediaProvier provider;
  MediaStreamHandlers handlers(&provider);

  talk_base::scoped_refptr<StreamCollectionImpl> collection(
      StreamCollectionImpl::Create());
  collection->AddStream(stream);

  EXPECT_CALL(provider, SetLocalRenderer(kVideoDeviceName))
      .Times(Exactly(2));  // SetLocalRender will also be called from dtor of
                           // LocalVideoTrackHandler
  EXPECT_CALL(provider, SetCaptureDevice(kVideoDeviceName))
      .Times(Exactly(1));
  handlers.CommitLocalStreams(collection);

  video_track->set_state(MediaStreamTrackInterface::kLive);
  // Process posted messages.
  talk_base::Thread::Current()->ProcessMessages(1);

  collection->RemoveStream(stream);
  handlers.CommitLocalStreams(collection);

  video_track->set_state(MediaStreamTrackInterface::kEnded);
  // Process posted messages.
  talk_base::Thread::Current()->ProcessMessages(1);
}

TEST(MediaStreamHandlerTest, RemoteStreams) {
  // Create a local stream. We use local stream in this test as well because
  // they are easier to create.
  // LocalMediaStreams inherit from MediaStreams.
  std::string label(kStreamLabel1);
  talk_base::scoped_refptr<LocalMediaStreamInterface> stream(
      MediaStream::Create(label));
  talk_base::scoped_refptr<LocalVideoTrackInterface>
      video_track(VideoTrack::CreateLocal(kVideoDeviceName, NULL));
  EXPECT_TRUE(stream->AddTrack(video_track));

  MockMediaProvier provider;
  MediaStreamHandlers handlers(&provider);

  handlers.AddRemoteStream(stream);

  EXPECT_CALL(provider, SetRemoteRenderer(kVideoDeviceName))
      .Times(Exactly(3));  // SetRemoteRenderer is also called from dtor of
                           // RemoteVideoTrackHandler.

  // Set the renderer once.
  talk_base::scoped_refptr<VideoRendererWrapperInterface> renderer(
      CreateVideoRenderer(NULL));
    video_track->SetRenderer(renderer);
  talk_base::Thread::Current()->ProcessMessages(1);

  // Change the already set renderer.
  renderer = CreateVideoRenderer(NULL);
    video_track->SetRenderer(renderer);
  talk_base::Thread::Current()->ProcessMessages(1);

  handlers.RemoveRemoteStream(stream);

  // Change the renderer after the stream have been removed from handler.
  // This should not trigger a call to SetRemoteRenderer.
  renderer = CreateVideoRenderer(NULL);
    video_track->SetRenderer(renderer);
  // Process posted messages.
  talk_base::Thread::Current()->ProcessMessages(1);
}

}  // namespace webrtc
