// libjingle
// Copyright 2004 Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. The name of the author may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Implementation of GtkVideoRenderer

#include "talk/media/devices/gtkvideorenderer.h"

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "talk/media/base/videocommon.h"
#include "talk/media/base/videoframe.h"

namespace cricket {

class ScopedGdkLock {
 public:
  ScopedGdkLock() {
    gdk_threads_enter();
  }

  ~ScopedGdkLock() {
    gdk_threads_leave();
  }
};

GtkVideoRenderer::GtkVideoRenderer(int x, int y)
    : window_(NULL),
      draw_area_(NULL),
      initial_x_(x),
      initial_y_(y) {
  g_type_init();
  g_thread_init(NULL);
  gdk_threads_init();
}

GtkVideoRenderer::~GtkVideoRenderer() {
  if (window_) {
    ScopedGdkLock lock;
    gtk_widget_destroy(window_);
    // Run the Gtk main loop to tear down the window.
    Pump();
  }
  // Don't need to destroy draw_area_ because it is not top-level, so it is
  // implicitly destroyed by the above.
}

bool GtkVideoRenderer::SetSize(int width, int height, int reserved) {
  ScopedGdkLock lock;

  // For the first frame, initialize the GTK window
  if ((!window_ && !Initialize(width, height)) || IsClosed()) {
    return false;
  }

  image_.reset(new uint8[width * height * 4]);
  gtk_widget_set_size_request(draw_area_, width, height);
  return true;
}

bool GtkVideoRenderer::RenderFrame(const VideoFrame* frame) {
  if (!frame) {
    return false;
  }

  // convert I420 frame to ABGR format, which is accepted by GTK
  frame->ConvertToRgbBuffer(cricket::FOURCC_ABGR,
                            image_.get(),
                            frame->GetWidth() * frame->GetHeight() * 4,
                            frame->GetWidth() * 4);

  ScopedGdkLock lock;

  if (IsClosed()) {
    return false;
  }

  // draw the ABGR image
  gdk_draw_rgb_32_image(draw_area_->window,
                        draw_area_->style->fg_gc[GTK_STATE_NORMAL],
                        0,
                        0,
                        frame->GetWidth(),
                        frame->GetHeight(),
                        GDK_RGB_DITHER_MAX,
                        image_.get(),
                        frame->GetWidth() * 4);

  // Run the Gtk main loop to refresh the window.
  Pump();
  return true;
}

bool GtkVideoRenderer::Initialize(int width, int height) {
  gtk_init(NULL, NULL);
  window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  draw_area_ = gtk_drawing_area_new();
  if (!window_ || !draw_area_) {
    return false;
  }

  gtk_window_set_position(GTK_WINDOW(window_), GTK_WIN_POS_CENTER);
  gtk_window_set_title(GTK_WINDOW(window_), "Video Renderer");
  gtk_window_set_resizable(GTK_WINDOW(window_), FALSE);
  gtk_widget_set_size_request(draw_area_, width, height);
  gtk_container_add(GTK_CONTAINER(window_), draw_area_);
  gtk_widget_show_all(window_);
  gtk_window_move(GTK_WINDOW(window_), initial_x_, initial_y_);

  image_.reset(new uint8[width * height * 4]);
  return true;
}

void GtkVideoRenderer::Pump() {
  while (gtk_events_pending()) {
    gtk_main_iteration();
  }
}

bool GtkVideoRenderer::IsClosed() const {
  if (!window_) {
    // Not initialized yet, so hasn't been closed.
    return false;
  }

  if (!GTK_IS_WINDOW(window_) || !GTK_IS_DRAWING_AREA(draw_area_)) {
    return true;
  }

  return false;
}

}  // namespace cricket
