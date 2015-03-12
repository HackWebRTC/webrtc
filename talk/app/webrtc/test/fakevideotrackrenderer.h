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

#ifndef TALK_APP_WEBRTC_TEST_FAKEVIDEOTRACKRENDERER_H_
#define TALK_APP_WEBRTC_TEST_FAKEVIDEOTRACKRENDERER_H_

#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/media/base/fakevideorenderer.h"

namespace webrtc {

class FakeVideoTrackRenderer : public VideoRendererInterface {
 public:
  FakeVideoTrackRenderer(VideoTrackInterface* video_track)
      : video_track_(video_track),
        can_apply_rotation_(true),
        last_frame_(NULL) {
    video_track_->AddRenderer(this);
  }
  FakeVideoTrackRenderer(VideoTrackInterface* video_track,
                         bool can_apply_rotation)
      : video_track_(video_track),
        can_apply_rotation_(can_apply_rotation),
        last_frame_(NULL) {
    video_track_->AddRenderer(this);
  }
  ~FakeVideoTrackRenderer() {
    video_track_->RemoveRenderer(this);
  }

  virtual void RenderFrame(const cricket::VideoFrame* video_frame) override {
    last_frame_ = const_cast<cricket::VideoFrame*>(video_frame);

    const cricket::VideoFrame* frame =
        can_apply_rotation_ ? video_frame
                            : video_frame->GetCopyWithRotationApplied();

    if (!fake_renderer_.SetSize(static_cast<int>(frame->GetWidth()),
                                static_cast<int>(frame->GetHeight()), 0)) {
      return;
    }

    fake_renderer_.RenderFrame(frame);
  }

  virtual bool CanApplyRotation() override { return can_apply_rotation_; }

  int errors() const { return fake_renderer_.errors(); }
  int width() const { return fake_renderer_.width(); }
  int height() const { return fake_renderer_.height(); }
  int num_rendered_frames() const {
    return fake_renderer_.num_rendered_frames();
  }
  const cricket::VideoFrame* last_frame() const { return last_frame_; }

 private:
  cricket::FakeVideoRenderer fake_renderer_;
  rtc::scoped_refptr<VideoTrackInterface> video_track_;
  bool can_apply_rotation_;

  // Weak reference for frame pointer comparison only.
  cricket::VideoFrame* last_frame_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_TEST_FAKEVIDEOTRACKRENDERER_H_
