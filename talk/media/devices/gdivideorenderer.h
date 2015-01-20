/*
 * libjingle
 * Copyright 2004 Google Inc.
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

//
// Definition of class GdiVideoRenderer that implements the abstract class
// cricket::VideoRenderer via GDI on Windows.

#ifndef TALK_MEDIA_DEVICES_GDIVIDEORENDERER_H_
#define TALK_MEDIA_DEVICES_GDIVIDEORENDERER_H_

#ifdef WIN32
#include "talk/media/base/videorenderer.h"
#include "webrtc/base/scoped_ptr.h"

namespace cricket {

class GdiVideoRenderer : public VideoRenderer {
 public:
  GdiVideoRenderer(int x, int y);
  virtual ~GdiVideoRenderer();

  // Implementation of pure virtual methods of VideoRenderer.
  // These two methods may be executed in different threads.
  // SetSize is called before RenderFrame.
  virtual bool SetSize(int width, int height, int reserved);
  virtual bool RenderFrame(const VideoFrame* frame);

 private:
  class VideoWindow;  // forward declaration, defined in the .cc file
  rtc::scoped_ptr<VideoWindow> window_;
  // The initial position of the window.
  int initial_x_;
  int initial_y_;
};

}  // namespace cricket

#endif  // WIN32
#endif  // TALK_MEDIA_DEVICES_GDIVIDEORENDERER_H_
