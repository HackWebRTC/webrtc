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

#include "talk/media/base/capturerenderadapter.h"

#include "talk/media/base/videocapturer.h"
#include "talk/media/base/videorenderer.h"
#include "webrtc/base/logging.h"

namespace cricket {

CaptureRenderAdapter::CaptureRenderAdapter(VideoCapturer* video_capturer)
    : video_capturer_(video_capturer) {
}

CaptureRenderAdapter::~CaptureRenderAdapter() {
  // Since the signal we're connecting to is multi-threaded,
  // disconnect_all() will block until all calls are serviced, meaning any
  // outstanding calls to OnVideoFrame will be done when this is done, and no
  // more calls will be serviced by this.
  // We do this explicitly instead of just letting the has_slots<> destructor
  // take care of it because we need to do this *before* video_renderers_ is
  // cleared by the destructor; otherwise we could mess with it while
  // OnVideoFrame is running.
  // We *don't* take capture_crit_ here since it could deadlock with the lock
  // taken by the video frame signal.
  disconnect_all();
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

void CaptureRenderAdapter::AddRenderer(VideoRenderer* video_renderer) {
  RTC_DCHECK(video_renderer);

  rtc::CritScope cs(&capture_crit_);
  // This implements set semantics, the same renderer can only be
  // added once.
  // TODO(nisse): Is this really needed?
  if (std::find(video_renderers_.begin(), video_renderers_.end(),
                video_renderer) == video_renderers_.end())
    video_renderers_.push_back(video_renderer);
}

void CaptureRenderAdapter::RemoveRenderer(VideoRenderer* video_renderer) {
  RTC_DCHECK(video_renderer);

  rtc::CritScope cs(&capture_crit_);
  // TODO(nisse): Switch to using std::list, and use its remove
  // method. And similarly in VideoTrackRenderers, which this class
  // mostly duplicates.
  for (VideoRenderers::iterator iter = video_renderers_.begin();
       iter != video_renderers_.end(); ++iter) {
    if (video_renderer == *iter) {
      video_renderers_.erase(iter);
      break;
    }
  }
}

void CaptureRenderAdapter::Init() {
  video_capturer_->SignalVideoFrame.connect(
      this,
      &CaptureRenderAdapter::OnVideoFrame);
}

void CaptureRenderAdapter::OnVideoFrame(VideoCapturer* capturer,
                                        const VideoFrame* video_frame) {
  rtc::CritScope cs(&capture_crit_);
  if (video_renderers_.empty()) {
    return;
  }

  for (auto* renderer : video_renderers_)
    renderer->RenderFrame(video_frame);
}

}  // namespace cricket
