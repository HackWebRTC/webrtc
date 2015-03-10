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

#ifndef TALK_APP_WEBRTC_VIDEOTRACKRENDERERS_H_
#define TALK_APP_WEBRTC_VIDEOTRACKRENDERERS_H_

#include <vector>

#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/media/base/videorenderer.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ptr.h"

namespace webrtc {

// Class used for rendering cricket::VideoFrames to multiple renderers of type
// VideoRendererInterface.
// Each VideoTrack owns a VideoTrackRenderers instance.
// The class is thread safe. Rendering to the added VideoRendererInterfaces is
// done on the same thread as the cricket::VideoRenderer.
class VideoTrackRenderers : public cricket::VideoRenderer {
 public:
  VideoTrackRenderers();
  ~VideoTrackRenderers();

  // Implements cricket::VideoRenderer
  virtual bool SetSize(int width, int height, int reserved);
  virtual bool RenderFrame(const cricket::VideoFrame* frame);

  void AddRenderer(VideoRendererInterface* renderer);
  void RemoveRenderer(VideoRendererInterface* renderer);
  void SetEnabled(bool enable);

 private:
  struct RenderObserver {
    explicit RenderObserver(VideoRendererInterface* renderer)
        : renderer_(renderer),
          can_apply_rotation_(renderer->CanApplyRotation()) {}
    VideoRendererInterface* renderer_;
    bool can_apply_rotation_;
  };

  bool enabled_;
  std::vector<RenderObserver> renderers_;

  rtc::CriticalSection critical_section_;  // Protects the above variables
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_VIDEOTRACKRENDERERS_H_
