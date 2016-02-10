/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/videotrackrenderers.h"
#include "webrtc/media/webrtc/webrtcvideoframe.h"

namespace webrtc {

VideoTrackRenderers::VideoTrackRenderers() : enabled_(true) {
}

VideoTrackRenderers::~VideoTrackRenderers() {
}

void VideoTrackRenderers::AddRenderer(VideoRendererInterface* renderer) {
  if (!renderer) {
    return;
  }
  rtc::CritScope cs(&critical_section_);
  renderers_.insert(renderer);
}

void VideoTrackRenderers::RemoveRenderer(VideoRendererInterface* renderer) {
  rtc::CritScope cs(&critical_section_);
  renderers_.erase(renderer);
}

void VideoTrackRenderers::SetEnabled(bool enable) {
  rtc::CritScope cs(&critical_section_);
  enabled_ = enable;
}

bool VideoTrackRenderers::RenderFrame(const cricket::VideoFrame* frame) {
  {
    rtc::CritScope cs(&critical_section_);
    if (enabled_) {
      RenderFrameToRenderers(frame);
      return true;
    }
  }

  // Generate the black frame outside of the critical section. Note
  // that this may result in unexpected frame order, in the unlikely
  // case that RenderFrame is called from multiple threads without
  // proper serialization, and the track is switched from disabled to
  // enabled in the middle of the first call.
  cricket::WebRtcVideoFrame black(new rtc::RefCountedObject<I420Buffer>(
                                      static_cast<int>(frame->GetWidth()),
                                      static_cast<int>(frame->GetHeight())),
                                  frame->GetTimeStamp(),
                                  frame->GetVideoRotation());
  black.SetToBlack();

  {
    rtc::CritScope cs(&critical_section_);
    // Check enabled_ flag again, since the track might have been
    // enabled while we generated the black frame. I think the
    // enabled-ness ought to be applied at the track output, and hence
    // an enabled track shouldn't send any blacked out frames.
    RenderFrameToRenderers(enabled_ ? frame : &black);

    return true;
  }
}

// Called with critical_section_ already locked
void VideoTrackRenderers::RenderFrameToRenderers(
    const cricket::VideoFrame* frame) {
  for (VideoRendererInterface* renderer : renderers_) {
    renderer->RenderFrame(frame);
  }
}

}  // namespace webrtc
