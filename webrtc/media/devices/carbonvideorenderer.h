/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Definition of class CarbonVideoRenderer that implements the abstract class
// cricket::VideoRenderer via Carbon.

#ifndef WEBRTC_MEDIA_DEVICES_CARBONVIDEORENDERER_H_
#define WEBRTC_MEDIA_DEVICES_CARBONVIDEORENDERER_H_

#include <Carbon/Carbon.h>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/media/base/videorenderer.h"

namespace cricket {

class CarbonVideoRenderer : public VideoRenderer {
 public:
  CarbonVideoRenderer(int x, int y);
  virtual ~CarbonVideoRenderer();

  // Implementation of pure virtual methods of VideoRenderer.
  // These two methods may be executed in different threads.
  // SetSize is called before RenderFrame.
  virtual bool SetSize(int width, int height, int reserved);
  virtual bool RenderFrame(const VideoFrame* frame);

  // Needs to be called on the main thread.
  bool Initialize();

 private:
  bool DrawFrame();

  static OSStatus DrawEventHandler(EventHandlerCallRef handler,
                                   EventRef event,
                                   void* data);
  rtc::scoped_ptr<uint8_t[]> image_;
  rtc::CriticalSection image_crit_;
  int image_width_;
  int image_height_;
  int x_;
  int y_;
  WindowRef window_ref_;
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_DEVICES_CARBONVIDEORENDERER_H_
