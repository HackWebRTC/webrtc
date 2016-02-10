/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_VIDEOTRACKRENDERERS_H_
#define WEBRTC_API_VIDEOTRACKRENDERERS_H_

#include <set>

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/media/base/videorenderer.h"

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

  // Implements cricket::VideoRenderer. If the track is disabled,
  // incoming frames are replaced by black frames.
  virtual bool RenderFrame(const cricket::VideoFrame* frame);

  void AddRenderer(VideoRendererInterface* renderer);
  void RemoveRenderer(VideoRendererInterface* renderer);
  void SetEnabled(bool enable);

 private:
  // Pass the frame on to to each registered renderer. Requires
  // critical_section_ already locked.
  void RenderFrameToRenderers(const cricket::VideoFrame* frame);

  bool enabled_;
  std::set<VideoRendererInterface*> renderers_;

  rtc::CriticalSection critical_section_;  // Protects the above variables
};

}  // namespace webrtc

#endif  // WEBRTC_API_VIDEOTRACKRENDERERS_H_
