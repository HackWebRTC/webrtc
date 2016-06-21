/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_DESKTOP_CAPTURER_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_DESKTOP_CAPTURER_H_

#include <stddef.h>

#include <memory>

#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "webrtc/modules/desktop_capture/shared_memory.h"

namespace webrtc {

class DesktopFrame;

// Abstract interface for screen and window capturers.
class DesktopCapturer {
 public:
  enum class Result {
    // The frame was captured successfully.
    SUCCESS,

    // There was a temporary error. The caller should continue calling
    // Capture(), in the expectation that it will eventually recover.
    ERROR_TEMPORARY,

    // Capture has failed and will keep failing if the caller tries calling
    // Capture() again.
    ERROR_PERMANENT,

    MAX_VALUE = ERROR_PERMANENT
  };

  // Interface that must be implemented by the DesktopCapturer consumers.
  class Callback {
   public:
    // Called after a frame has been captured. |frame| is not nullptr if and
    // only if |result| is SUCCESS.
    virtual void OnCaptureResult(Result result,
                                 std::unique_ptr<DesktopFrame> frame) = 0;

   protected:
    virtual ~Callback() {}
  };

  virtual ~DesktopCapturer() {}

  // Called at the beginning of a capturing session. |callback| must remain
  // valid until capturer is destroyed.
  virtual void Start(Callback* callback) = 0;

  // Sets SharedMemoryFactory that will be used to create buffers for the
  // captured frames. The factory can be invoked on a thread other than the one
  // where Capture() is called. It will be destroyed on the same thread. Shared
  // memory is currently supported only by some DesktopCapturer implementations.
  virtual void SetSharedMemoryFactory(
      std::unique_ptr<SharedMemoryFactory> shared_memory_factory) {}

  // Captures next frame. |region| specifies region of the capture target that
  // should be fresh in the resulting frame. The frame may also include fresh
  // data for areas outside |region|. In that case capturer will include these
  // areas in updated_region() of the frame. |region| is specified relative to
  // the top left corner of the capture target. Pending capture operations are
  // canceled when DesktopCapturer is deleted.
  virtual void Capture(const DesktopRegion& region) = 0;

  // Sets the window to be excluded from the captured image in the future
  // Capture calls. Used to exclude the screenshare notification window for
  // screen capturing.
  virtual void SetExcludedWindow(WindowId window) {}
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_DESKTOP_CAPTURER_H_

