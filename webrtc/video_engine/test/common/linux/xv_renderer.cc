/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/test/common/linux/xv_renderer.h"

#include <X11/Xutil.h>
#include <X11/extensions/Xvlib.h>
#include <sys/shm.h>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"

#define GUID_I420_PLANAR 0x30323449

namespace webrtc {
namespace test {

XvRenderer::XvRenderer(size_t width, size_t height)
    : width(width),
      height(height),
      is_init(false),
      display(NULL),
      gc(NULL),
      image(NULL) {
  assert(width > 0);
  assert(height > 0);
}

bool XvRenderer::Init(const char* window_title) {
  assert(!is_init);
  is_init = true;
  if ((display = XOpenDisplay(NULL)) == NULL) {
    Destroy();
    return false;
  }

  int screen = DefaultScreen(display);

  XVisualInfo vinfo;
  if (!XMatchVisualInfo(display, screen, 24, TrueColor, &vinfo)) {
    Destroy();
    return false;
  }

  XSetWindowAttributes xswa;
  xswa.colormap = XCreateColormap(display, DefaultRootWindow(display),
                                  vinfo.visual, AllocNone);
  xswa.event_mask = StructureNotifyMask | ExposureMask;
  xswa.background_pixel = 0;
  xswa.border_pixel = 0;

  window = XCreateWindow(display, DefaultRootWindow(display), 0, 0, width,
                         height, 0, vinfo.depth, InputOutput, vinfo.visual,
                         CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
                         &xswa);

  XStoreName(display, window, window_title);
  XSetIconName(display, window, window_title);

  XSelectInput(display, window, StructureNotifyMask);

  XMapRaised(display, window);

  XEvent event;
  do {
    XNextEvent(display, &event);
  } while (event.type != MapNotify || event.xmap.event != window);

  if (!XShmQueryExtension(display)) {
    Destroy();
    return false;
  }

  xv_complete = XShmGetEventBase(display) + ShmCompletion;

  XvAdaptorInfo* ai;
  unsigned int p_num_adaptors;

  if (XvQueryAdaptors(display, DefaultRootWindow(display), &p_num_adaptors,
                      &ai) !=
      Success) {
    Destroy();
    return false;
  }
  if (p_num_adaptors <= 0) {
    XvFreeAdaptorInfo(ai);
    Destroy();
    return false;
  }

  xv_port = ai[p_num_adaptors - 1].base_id;
  XvFreeAdaptorInfo(ai);

  if (xv_port == -1) {
    Destroy();
    return false;
  }

  gc = XCreateGC(display, window, 0, 0);
  if (gc == NULL) {
    Destroy();
    return false;
  }

  Resize(width, height);

  return true;
}

void XvRenderer::Destroy() {
  if (image != NULL) {
    XFree(image);
    image = NULL;
  }

  if (gc != NULL) {
    XFreeGC(display, gc);
    gc = NULL;
  }

  if (display != NULL) {
    XCloseDisplay(display);
    display = NULL;
  }
}

XvRenderer* XvRenderer::Create(const char* window_title, size_t width,
                               size_t height) {
  XvRenderer* xv_renderer = new XvRenderer(width, height);
  if (!xv_renderer->Init(window_title)) {
    // TODO(pbos): Add Xv-failed warning here?
    delete xv_renderer;
    return NULL;
  }
  return xv_renderer;
}

XvRenderer::~XvRenderer() { Destroy(); }

void XvRenderer::Resize(size_t width, size_t height) {
  this->width = width;
  this->height = height;

  if (image != NULL) {
    XFree(image);
  }
  image = XvShmCreateImage(display, xv_port, GUID_I420_PLANAR, 0, width, height,
                           &shm_info);
  assert(image != NULL);

  shm_info.shmid = shmget(IPC_PRIVATE, image->data_size, IPC_CREAT | 0777);
  shm_info.shmaddr = image->data =
      reinterpret_cast<char*>(shmat(shm_info.shmid, 0, 0));
  shm_info.readOnly = False;

  if (!XShmAttach(display, &shm_info)) {
    abort();
  }

  XSizeHints* size_hints = XAllocSizeHints();
  if (size_hints == NULL) {
    abort();
  }
  size_hints->flags = PAspect;
  size_hints->min_aspect.x = size_hints->max_aspect.x = width;
  size_hints->min_aspect.y = size_hints->max_aspect.y = height;
  XSetWMNormalHints(display, window, size_hints);
  XFree(size_hints);

  XWindowChanges wc;
  wc.width = width;
  wc.height = height;
  XConfigureWindow(display, window, CWWidth | CWHeight, &wc);
}

void XvRenderer::RenderFrame(const webrtc::I420VideoFrame& frame,
                             int /*render_delay_ms*/) {
  int size = webrtc::ExtractBuffer(frame, image->data_size,
                                   reinterpret_cast<uint8_t*>(image->data));
  if (static_cast<size_t>(frame.width()) != width ||
      static_cast<size_t>(frame.height()) != height) {
    Resize(static_cast<size_t>(frame.width()),
           static_cast<size_t>(frame.height()));
  }
  assert(size > 0);
  Window root;
  int temp;
  unsigned int window_width, window_height, u_temp;

  XGetGeometry(display, window, &root, &temp, &temp, &window_width,
               &window_height, &u_temp, &u_temp);

  XvShmPutImage(display, xv_port, window, gc, image, 0, 0, image->width,
                image->height, 0, 0, window_width, window_height, True);

  XFlush(display);

  XEvent event;
  while (XPending(display)) {
    XNextEvent(display, &event);
  }
}
}  // test
}  // webrtc
