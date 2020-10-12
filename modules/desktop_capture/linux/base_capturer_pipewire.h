/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_BASE_CAPTURER_PIPEWIRE_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_BASE_CAPTURER_PIPEWIRE_H_

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/linux/xdg_desktop_portal_base.h"

#include "api/ref_counted_base.h"
#include "rtc_base/constructor_magic.h"

namespace webrtc {

class RTC_EXPORT BaseCapturerPipeWire : public DesktopCapturer {
 public:
  explicit BaseCapturerPipeWire();
  ~BaseCapturerPipeWire() override;

  bool Init(const DesktopCaptureOptions& options,
            XdgDesktopPortalBase::CaptureSourceType type);

  void Start(Callback* delegate) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

  static std::unique_ptr<DesktopCapturer> CreateRawScreenCapturer(
      const DesktopCaptureOptions& options);

  static std::unique_ptr<DesktopCapturer> CreateRawWindowCapturer(
      const DesktopCaptureOptions& options);

 private:
  DesktopCaptureOptions options_ = {};
  Callback* callback_ = nullptr;

  XdgDesktopPortalBase::CaptureSourceType type_ =
      XdgDesktopPortalBase::CaptureSourceType::kScreen;
  absl::optional<int32_t> id_;
  bool auto_close_connection_ = false;
  bool portal_initialized_ = false;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_BASE_CAPTURER_PIPEWIRE_H_
