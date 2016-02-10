/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_TEST_FAKEVIDEOTRACKRENDERER_H_
#define WEBRTC_API_TEST_FAKEVIDEOTRACKRENDERER_H_

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/media/base/fakevideorenderer.h"

namespace webrtc {

class FakeVideoTrackRenderer : public VideoRendererInterface {
 public:
  FakeVideoTrackRenderer(VideoTrackInterface* video_track)
      : video_track_(video_track), last_frame_(NULL) {
    video_track_->AddRenderer(this);
  }
  ~FakeVideoTrackRenderer() {
    video_track_->RemoveRenderer(this);
  }

  virtual void RenderFrame(const cricket::VideoFrame* video_frame) override {
    last_frame_ = const_cast<cricket::VideoFrame*>(video_frame);
    fake_renderer_.RenderFrame(video_frame);
  }

  int errors() const { return fake_renderer_.errors(); }
  int width() const { return fake_renderer_.width(); }
  int height() const { return fake_renderer_.height(); }
  bool black_frame() const { return fake_renderer_.black_frame(); }

  int num_rendered_frames() const {
    return fake_renderer_.num_rendered_frames();
  }
  const cricket::VideoFrame* last_frame() const { return last_frame_; }

 private:
  cricket::FakeVideoRenderer fake_renderer_;
  rtc::scoped_refptr<VideoTrackInterface> video_track_;

  // Weak reference for frame pointer comparison only.
  cricket::VideoFrame* last_frame_;
};

}  // namespace webrtc

#endif  // WEBRTC_API_TEST_FAKEVIDEOTRACKRENDERER_H_
