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

#include "talk/app/webrtc/videotrackrenderers.h"
#include "talk/media/base/videoframe.h"

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
  rtc::CritScope cs(&critical_section_);
  if (!enabled_) {
    return true;
  }
  for (VideoRendererInterface* renderer : renderers_) {
    renderer->RenderFrame(frame);
  }
  return true;
}

}  // namespace webrtc
