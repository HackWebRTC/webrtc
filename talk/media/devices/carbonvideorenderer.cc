// libjingle
// Copyright 2011 Google Inc.
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
// Implementation of CarbonVideoRenderer

#include "talk/media/devices/carbonvideorenderer.h"

#include "talk/base/logging.h"
#include "talk/media/base/videocommon.h"
#include "talk/media/base/videoframe.h"

namespace cricket {

CarbonVideoRenderer::CarbonVideoRenderer(int x, int y)
    : image_width_(0),
      image_height_(0),
      x_(x),
      y_(y),
      image_ref_(NULL),
      window_ref_(NULL) {
}

CarbonVideoRenderer::~CarbonVideoRenderer() {
  if (window_ref_) {
    DisposeWindow(window_ref_);
  }
}

// Called from the main event loop. All renderering needs to happen on
// the main thread.
OSStatus CarbonVideoRenderer::DrawEventHandler(EventHandlerCallRef handler,
                                               EventRef event,
                                               void* data) {
  OSStatus status = noErr;
  CarbonVideoRenderer* renderer = static_cast<CarbonVideoRenderer*>(data);
  if (renderer != NULL) {
    if (!renderer->DrawFrame()) {
      LOG(LS_ERROR) << "Failed to draw frame.";
    }
  }
  return status;
}

bool CarbonVideoRenderer::DrawFrame() {
  // Grab the image lock to make sure it is not changed why we'll draw it.
  talk_base::CritScope cs(&image_crit_);

  if (image_.get() == NULL) {
    // Nothing to draw, just return.
    return true;
  }
  int width = image_width_;
  int height = image_height_;
  CGDataProviderRef provider =
      CGDataProviderCreateWithData(NULL, image_.get(), width * height * 4,
                                   NULL);
  CGColorSpaceRef color_space_ref = CGColorSpaceCreateDeviceRGB();
  CGBitmapInfo bitmap_info = kCGBitmapByteOrderDefault;
  CGColorRenderingIntent rendering_intent = kCGRenderingIntentDefault;
  CGImageRef image_ref = CGImageCreate(width, height, 8, 32, width * 4,
                                       color_space_ref, bitmap_info, provider,
                                       NULL, false, rendering_intent);
  CGDataProviderRelease(provider);

  if (image_ref == NULL) {
    return false;
  }
  CGContextRef context;
  SetPortWindowPort(window_ref_);
  if (QDBeginCGContext(GetWindowPort(window_ref_), &context) != noErr) {
    CGImageRelease(image_ref);
    return false;
  }
  Rect window_bounds;
  GetWindowPortBounds(window_ref_, &window_bounds);

  // Anchor the image to the top left corner.
  int x = 0;
  int y = window_bounds.bottom - CGImageGetHeight(image_ref);
  CGRect dst_rect = CGRectMake(x, y, CGImageGetWidth(image_ref),
                               CGImageGetHeight(image_ref));
  CGContextDrawImage(context, dst_rect, image_ref);
  CGContextFlush(context);
  QDEndCGContext(GetWindowPort(window_ref_), &context);
  CGImageRelease(image_ref);
  return true;
}

bool CarbonVideoRenderer::SetSize(int width, int height, int reserved) {
  if (width != image_width_ || height != image_height_) {
    // Grab the image lock while changing its size.
    talk_base::CritScope cs(&image_crit_);
    image_width_ = width;
    image_height_ = height;
    image_.reset(new uint8[width * height * 4]);
    memset(image_.get(), 255, width * height * 4);
  }
  return true;
}

bool CarbonVideoRenderer::RenderFrame(const VideoFrame* frame) {
  if (!frame) {
    return false;
  }
  {
    // Grab the image lock so we are not trashing up the image being drawn.
    talk_base::CritScope cs(&image_crit_);
    frame->ConvertToRgbBuffer(cricket::FOURCC_ABGR,
                              image_.get(),
                              frame->GetWidth() * frame->GetHeight() * 4,
                              frame->GetWidth() * 4);
  }

  // Trigger a repaint event for the whole window.
  Rect bounds;
  InvalWindowRect(window_ref_, GetWindowPortBounds(window_ref_, &bounds));
  return true;
}

bool CarbonVideoRenderer::Initialize() {
  OSStatus err;
  WindowAttributes attributes =
      kWindowStandardDocumentAttributes |
      kWindowLiveResizeAttribute |
      kWindowFrameworkScaledAttribute |
      kWindowStandardHandlerAttribute;

  struct Rect bounds;
  bounds.top = y_;
  bounds.bottom = 480;
  bounds.left = x_;
  bounds.right = 640;
  err = CreateNewWindow(kDocumentWindowClass, attributes,
                        &bounds, &window_ref_);
  if (!window_ref_ || err != noErr) {
    LOG(LS_ERROR) << "CreateNewWindow failed, error code: " << err;
    return false;
  }
  static const EventTypeSpec event_spec = {
    kEventClassWindow,
    kEventWindowDrawContent
  };

  err = InstallWindowEventHandler(
      window_ref_,
      NewEventHandlerUPP(CarbonVideoRenderer::DrawEventHandler),
      GetEventTypeCount(event_spec),
      &event_spec,
      this,
      NULL);
  if (err != noErr) {
    LOG(LS_ERROR) << "Failed to install event handler, error code: " << err;
    return false;
  }
  SelectWindow(window_ref_);
  ShowWindow(window_ref_);
  return true;
}

}  // namespace cricket
