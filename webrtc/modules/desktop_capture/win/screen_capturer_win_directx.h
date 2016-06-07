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

#include <comdef.h>
#include <D3DCommon.h>
#include <D3D11.h>
#include <DXGI.h>
#include <DXGI1_2.h>
#include <windows.h>
#include <wrl/client.h>

#include <memory>
#include <vector>

#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "webrtc/modules/desktop_capture/desktop_geometry.h"
#include "webrtc/modules/desktop_capture/desktop_region.h"
#include "webrtc/modules/desktop_capture/screen_capture_frame_queue.h"
#include "webrtc/modules/desktop_capture/shared_desktop_frame.h"

namespace webrtc {

// ScreenCapturerWinDirectx captures 32bit RGBA using DirectX. This
// implementation won't work when ScreenCaptureFrameQueue.kQueueLength is not 2.
class ScreenCapturerWinDirectx : public ScreenCapturer {
 public:
  // Initializes DirectX related components. Returns false if any error
  // happened, any instance of this class won't be able to work in such status.
  // Thread safe, guarded by initialize_lock.
  static bool Initialize();

  explicit ScreenCapturerWinDirectx(const DesktopCaptureOptions& options);
  virtual ~ScreenCapturerWinDirectx();

  void Start(Callback* callback) override;
  void SetSharedMemoryFactory(
      std::unique_ptr<SharedMemoryFactory> shared_memory_factory) override;
  void Capture(const DesktopRegion& region) override;
  bool GetScreenList(ScreenList* screens) override;
  bool SelectScreen(ScreenId id) override;

 private:
  // Texture is a pair of an ID3D11Texture2D and an IDXGISurface. Refer to its
  // implementation in source code for details.
  class Texture;

  // An implementation of DesktopFrame to return data from a Texture instance.
  class DxgiDesktopFrame;

  static bool DoInitialize();

  // Initializes DxgiOutputDuplication. If current DxgiOutputDuplication
  // instance is existing, this function takes no-op and returns true. Returns
  // false if it fails to execute windows api.
  static bool DuplicateOutput();

  // Deprecates current DxgiOutputDuplication instance and calls DuplicateOutput
  // to reinitialize it.
  static bool ForceDuplicateOutput();

  // Detects update regions in last frame, if anything wrong, returns false.
  // ProcessFrame will insert a whole desktop size as updated region instead.
  static bool DetectUpdatedRegion(const DXGI_OUTDUPL_FRAME_INFO& frame_info,
                                  DesktopRegion* updated_region);

  // A helper function to handle _com_error result in DetectUpdatedRegion.
  // Returns false if the _com_error shows an error.
  static bool HandleDetectUpdatedRegionError(const _com_error& error,
                                             const char* stage);

  // Processes one frame received from AcquireNextFrame function, returns a
  // nullptr if anything wrong.
  std::unique_ptr<DesktopFrame> ProcessFrame(
      const DXGI_OUTDUPL_FRAME_INFO& frame_info,
      IDXGIResource* resource);

  // A shortcut to execute callback with current frame in frames.
  void EmitCurrentFrame();

  ScreenCaptureFrameQueue<rtc::scoped_refptr<Texture>> surfaces_;
  ScreenCaptureFrameQueue<SharedDesktopFrame> frames_;
  std::unique_ptr<SharedMemoryFactory> shared_memory_factory_;
  Callback* callback_ = nullptr;

  bool set_thread_execution_state_failed_ = false;

  RTC_DISALLOW_COPY_AND_ASSIGN(ScreenCapturerWinDirectx);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURER_WIN_DIRECTX_H_
