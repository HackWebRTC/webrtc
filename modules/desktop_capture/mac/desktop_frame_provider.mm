/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/mac/desktop_frame_provider.h"

#include <utility>

#include "modules/desktop_capture/mac/desktop_frame_cgimage.h"
#include "modules/desktop_capture/mac/desktop_frame_iosurface.h"

namespace webrtc {

DesktopFrameProvider::DesktopFrameProvider(bool allow_iosurface)
    : allow_iosurface_(allow_iosurface), io_surfaces_lock_(RWLockWrapper::CreateRWLock()) {}

DesktopFrameProvider::~DesktopFrameProvider() {
  // Might be called from a thread which is not the one running the CGDisplayStream
  // handler. Indeed chromium's content destroys it from a dedicated thread.
  Release();
}

std::unique_ptr<DesktopFrame> DesktopFrameProvider::TakeLatestFrameForDisplay(
    CGDirectDisplayID display_id) {
  if (!allow_iosurface_) {
    // Regenerate a snapshot.
    return DesktopFrameCGImage::CreateForDisplay(display_id);
  }

  // Might be called from a thread which is not the one running the CGDisplayStream
  // handler. Indeed chromium's content uses a dedicates thread.
  WriteLockScoped scoped_io_surfaces_lock(*io_surfaces_lock_);
  if (io_surfaces_[display_id]) {
    return std::move(io_surfaces_[display_id]);
  }

  return nullptr;
}

void DesktopFrameProvider::InvalidateIOSurface(CGDirectDisplayID display_id,
                                               rtc::ScopedCFTypeRef<IOSurfaceRef> io_surface) {
  if (!allow_iosurface_) {
    return;
  }

  std::unique_ptr<DesktopFrameIOSurface> desktop_frame_iosurface =
      DesktopFrameIOSurface::Wrap(io_surface);

  // Call from the thread which runs the CGDisplayStream handler.
  WriteLockScoped scoped_io_surfaces_lock(*io_surfaces_lock_);
  io_surfaces_[display_id] = std::move(desktop_frame_iosurface);
}

void DesktopFrameProvider::Release() {
  if (!allow_iosurface_) {
    return;
  }

  WriteLockScoped scoped_io_surfaces_lock(*io_surfaces_lock_);
  io_surfaces_.clear();
}

}  // namespace webrtc
