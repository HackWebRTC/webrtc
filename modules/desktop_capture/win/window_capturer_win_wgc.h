/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_WINDOW_CAPTURER_WIN_WGC_H_
#define MODULES_DESKTOP_CAPTURE_WIN_WINDOW_CAPTURER_WIN_WGC_H_

#include <memory>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"

namespace webrtc {

class WindowCapturerWgc : public DesktopCapturer {
 public:
  WindowCapturerWgc();

  // Disallow copy and assign
  WindowCapturerWgc(const WindowCapturerWgc&) = delete;
  WindowCapturerWgc& operator=(const WindowCapturerWgc&) = delete;

  ~WindowCapturerWgc() override;

  static std::unique_ptr<DesktopCapturer> CreateRawWindowCapturer(
      const DesktopCaptureOptions& options);

  // DesktopCapturer interface.
  void Start(Callback* callback) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

 private:
  Callback* callback_ = nullptr;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_WIN_WINDOW_CAPTURER_WIN_WGC_H_
