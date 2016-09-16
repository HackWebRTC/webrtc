/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURER_WIN_DIRECTX_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURER_WIN_DIRECTX_H_

#include "webrtc/modules/desktop_capture/screen_capturer.h"

#include <memory>
#include <vector>

#include "webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "webrtc/modules/desktop_capture/desktop_region.h"
#include "webrtc/modules/desktop_capture/screen_capture_frame_queue.h"
#include "webrtc/modules/desktop_capture/shared_desktop_frame.h"
#include "webrtc/modules/desktop_capture/win/dxgi_duplicator_controller.h"

namespace webrtc {

// ScreenCapturerWinDirectx captures 32bit RGBA using DirectX. This
// implementation won't work when ScreenCaptureFrameQueue.kQueueLength is not 2.
class ScreenCapturerWinDirectx : public ScreenCapturer {
 public:
  // Whether the system supports DirectX based capturing.
  static bool IsSupported();

  explicit ScreenCapturerWinDirectx(const DesktopCaptureOptions& options);

  virtual ~ScreenCapturerWinDirectx();

  void Start(Callback* callback) override;
  void SetSharedMemoryFactory(
      std::unique_ptr<SharedMemoryFactory> shared_memory_factory) override;
  void Capture(const DesktopRegion& region) override;
  bool GetScreenList(ScreenList* screens) override;
  bool SelectScreen(ScreenId id) override;

 private:
  // Returns desktop size of selected screen.
  DesktopSize SelectedDesktopSize() const;

  ScreenCaptureFrameQueue<SharedDesktopFrame> frames_;
  std::unique_ptr<SharedMemoryFactory> shared_memory_factory_;
  Callback* callback_ = nullptr;

  DxgiDuplicatorController::Context context_;

  ScreenId current_screen_id = kFullDesktopScreenId;

  RTC_DISALLOW_COPY_AND_ASSIGN(ScreenCapturerWinDirectx);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURER_WIN_DIRECTX_H_
