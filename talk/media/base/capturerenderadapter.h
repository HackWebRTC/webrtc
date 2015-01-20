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

// This file contains the class CaptureRenderAdapter. The class connects a
// VideoCapturer to any number of VideoRenders such that the former feeds the
// latter.
// CaptureRenderAdapter is Thread-unsafe. This means that none of its APIs may
// be called concurrently.

#ifndef TALK_MEDIA_BASE_CAPTURERENDERADAPTER_H_
#define TALK_MEDIA_BASE_CAPTURERENDERADAPTER_H_

#include <vector>

#include "talk/media/base/videocapturer.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/sigslot.h"

namespace cricket {

class VideoCapturer;
class VideoProcessor;
class VideoRenderer;

class CaptureRenderAdapter : public sigslot::has_slots<> {
 public:
  static CaptureRenderAdapter* Create(VideoCapturer* video_capturer);
  ~CaptureRenderAdapter();

  bool AddRenderer(VideoRenderer* video_renderer);
  bool RemoveRenderer(VideoRenderer* video_renderer);

  VideoCapturer* video_capturer() { return video_capturer_; }
 private:
  struct VideoRendererInfo {
    explicit VideoRendererInfo(VideoRenderer* r)
        : renderer(r),
          render_width(0),
          render_height(0) {
    }
    VideoRenderer* renderer;
    size_t render_width;
    size_t render_height;
  };

  // Just pointers since ownership is not handed over to this class.
  typedef std::vector<VideoRendererInfo> VideoRenderers;

  explicit CaptureRenderAdapter(VideoCapturer* video_capturer);
  void Init();

  // Callback for frames received from the capturer.
  void OnVideoFrame(VideoCapturer* capturer, const VideoFrame* video_frame);

  void MaybeSetRenderingSize(const VideoFrame* frame);

  bool IsRendererRegistered(const VideoRenderer& video_renderer) const;

  VideoRenderers video_renderers_;
  VideoCapturer* video_capturer_;
  // Critical section synchronizing the capture thread.
  mutable rtc::CriticalSection capture_crit_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_CAPTURERENDERADAPTER_H_
