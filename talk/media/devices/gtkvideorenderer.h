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

// Definition of class GtkVideoRenderer that implements the abstract class
// cricket::VideoRenderer via GTK.

#ifndef TALK_MEDIA_DEVICES_GTKVIDEORENDERER_H_
#define TALK_MEDIA_DEVICES_GTKVIDEORENDERER_H_

#include "talk/media/base/videorenderer.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/base/scoped_ptr.h"

typedef struct _GtkWidget GtkWidget;  // forward declaration, defined in gtk.h

namespace cricket {

class GtkVideoRenderer : public VideoRenderer {
 public:
  GtkVideoRenderer(int x, int y);
  virtual ~GtkVideoRenderer();

  // Implementation of pure virtual methods of VideoRenderer.
  // These two methods may be executed in different threads.
  // SetSize is called before RenderFrame.
  virtual bool SetSize(int width, int height, int reserved);
  virtual bool RenderFrame(const VideoFrame* frame);

 private:
  // Initialize the attributes when the first frame arrives.
  bool Initialize(int width, int height);
  // Pump the Gtk event loop until there are no events left.
  void Pump();
  // Check if the window has been closed.
  bool IsClosed() const;

  rtc::scoped_ptr<uint8_t[]> image_;
  GtkWidget* window_;
  GtkWidget* draw_area_;
  // The initial position of the window.
  int initial_x_;
  int initial_y_;

  int width_;
  int height_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_DEVICES_GTKVIDEORENDERER_H_
