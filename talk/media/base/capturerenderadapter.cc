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

#include "talk/media/base/capturerenderadapter.h"

#include "talk/base/logging.h"
#include "talk/media/base/videocapturer.h"
#include "talk/media/base/videoprocessor.h"
#include "talk/media/base/videorenderer.h"

namespace cricket {

CaptureRenderAdapter::CaptureRenderAdapter(VideoCapturer* video_capturer)
    : video_capturer_(video_capturer) {
}

CaptureRenderAdapter::~CaptureRenderAdapter() {
  // has_slots destructor will disconnect us from any signals we may be
  // connected to.
}

CaptureRenderAdapter* CaptureRenderAdapter::Create(
    VideoCapturer* video_capturer) {
  if (!video_capturer) {
    return NULL;
  }
  CaptureRenderAdapter* return_value = new CaptureRenderAdapter(video_capturer);
  return_value->Init();  // Can't fail.
  return return_value;
}

bool CaptureRenderAdapter::AddRenderer(VideoRenderer* video_renderer) {
  if (!video_renderer) {
    return false;
  }
  talk_base::CritScope cs(&capture_crit_);
  if (IsRendererRegistered(*video_renderer)) {
    return false;
  }
  video_renderers_.push_back(VideoRendererInfo(video_renderer));
  return true;
}

bool CaptureRenderAdapter::RemoveRenderer(VideoRenderer* video_renderer) {
  if (!video_renderer) {
    return false;
  }
  talk_base::CritScope cs(&capture_crit_);
  for (VideoRenderers::iterator iter = video_renderers_.begin();
       iter != video_renderers_.end(); ++iter) {
    if (video_renderer == iter->renderer) {
      video_renderers_.erase(iter);
      return true;
    }
  }
  return false;
}

void CaptureRenderAdapter::Init() {
  video_capturer_->SignalVideoFrame.connect(
      this,
      &CaptureRenderAdapter::OnVideoFrame);
}

void CaptureRenderAdapter::OnVideoFrame(VideoCapturer* capturer,
                                        const VideoFrame* video_frame) {
  talk_base::CritScope cs(&capture_crit_);
  if (video_renderers_.empty()) {
    return;
  }
  MaybeSetRenderingSize(video_frame);

  for (VideoRenderers::iterator iter = video_renderers_.begin();
       iter != video_renderers_.end(); ++iter) {
    VideoRenderer* video_renderer = iter->renderer;
    video_renderer->RenderFrame(video_frame);
  }
}

// The renderer_crit_ lock needs to be taken when calling this function.
void CaptureRenderAdapter::MaybeSetRenderingSize(const VideoFrame* frame) {
  for (VideoRenderers::iterator iter = video_renderers_.begin();
       iter != video_renderers_.end(); ++iter) {
    const bool new_resolution = iter->render_width != frame->GetWidth() ||
        iter->render_height != frame->GetHeight();
    if (new_resolution) {
      if (iter->renderer->SetSize(static_cast<int>(frame->GetWidth()),
                                  static_cast<int>(frame->GetHeight()), 0)) {
        iter->render_width = frame->GetWidth();
        iter->render_height = frame->GetHeight();
      } else {
        LOG(LS_ERROR) << "Captured frame size not supported by renderer: " <<
            frame->GetWidth() << " x " << frame->GetHeight();
      }
    }
  }
}

// The renderer_crit_ lock needs to be taken when calling this function.
bool CaptureRenderAdapter::IsRendererRegistered(
    const VideoRenderer& video_renderer) const {
  for (VideoRenderers::const_iterator iter = video_renderers_.begin();
       iter != video_renderers_.end(); ++iter) {
    if (&video_renderer == iter->renderer) {
      return true;
    }
  }
  return false;
}

}  // namespace cricket
