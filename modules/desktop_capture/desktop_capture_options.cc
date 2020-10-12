/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/desktop_capture_options.h"
#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
#include "modules/desktop_capture/mac/full_screen_mac_application_handler.h"
#elif defined(WEBRTC_WIN)
#include "modules/desktop_capture/win/full_screen_win_application_handler.h"
#endif
#if defined(WEBRTC_USE_PIPEWIRE)
#include "modules/desktop_capture/linux/xdg_desktop_portal_base.h"
#endif

namespace webrtc {

DesktopCaptureOptions::DesktopCaptureOptions() {}
DesktopCaptureOptions::DesktopCaptureOptions(
    const DesktopCaptureOptions& options) = default;
DesktopCaptureOptions::DesktopCaptureOptions(DesktopCaptureOptions&& options) =
    default;
DesktopCaptureOptions::~DesktopCaptureOptions() {}

DesktopCaptureOptions& DesktopCaptureOptions::operator=(
    const DesktopCaptureOptions& options) = default;
DesktopCaptureOptions& DesktopCaptureOptions::operator=(
    DesktopCaptureOptions&& options) = default;

// static
DesktopCaptureOptions DesktopCaptureOptions::CreateDefault() {
  DesktopCaptureOptions result;
#if defined(WEBRTC_USE_X11)
  result.set_x_display(SharedXDisplay::CreateDefault());
#endif
#if defined(WEBRTC_USE_PIPEWIRE)
  result.set_xdp_base(XdgDesktopPortalBase::CreateDefault());
#endif
#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
  result.set_configuration_monitor(new DesktopConfigurationMonitor());
  result.set_full_screen_window_detector(
      new FullScreenWindowDetector(CreateFullScreenMacApplicationHandler));
#elif defined(WEBRTC_WIN)
  result.set_full_screen_window_detector(
      new FullScreenWindowDetector(CreateFullScreenWinApplicationHandler));
#endif
  return result;
}

#if defined(WEBRTC_USE_PIPEWIRE)
void DesktopCaptureOptions::start_request(int32_t request_id) {
  // In case we get a duplicit start_request call, which might happen when a
  // browser requests both screen and window sharing, we don't want to do
  // anything.
  if (request_id == xdp_base_->CurrentConnectionId()) {
    return;
  }

  // In case we are about to start a new request and the previous one is not
  // finalized and not stream to the web page itself we will just close it.
  if (!xdp_base_->IsConnectionStreamingOnWeb(absl::nullopt) &&
      xdp_base_->IsConnectionInitialized(absl::nullopt)) {
    xdp_base_->CloseConnection(absl::nullopt);
  }

  xdp_base_->SetCurrentConnectionId(request_id);
}

void DesktopCaptureOptions::close_request(int32_t request_id) {
  xdp_base_->CloseConnection(request_id);
  xdp_base_->SetCurrentConnectionId(absl::nullopt);
}

absl::optional<int32_t> DesktopCaptureOptions::request_id() {
  // Reset request_id in case the connection is in final state, which means it
  // is streaming content to the web page itself and nobody should be asking
  // again for this ID.
  if (xdp_base_->IsConnectionStreamingOnWeb(absl::nullopt)) {
    xdp_base_->SetCurrentConnectionId(absl::nullopt);
  }

  return xdp_base_->CurrentConnectionId();
}

XdgDesktopPortalBase* DesktopCaptureOptions::xdp_base() const {
  return xdp_base_;
}

void DesktopCaptureOptions::set_xdp_base(
    rtc::scoped_refptr<XdgDesktopPortalBase> xdp_base) {
  xdp_base_ = std::move(xdp_base);
}
#endif

}  // namespace webrtc
