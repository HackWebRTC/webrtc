/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/base_capturer_pipewire.h"

#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/linux/pipewire_base.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

BaseCapturerPipeWire::BaseCapturerPipeWire() {}

BaseCapturerPipeWire::~BaseCapturerPipeWire() {
  webrtc::XdgDesktopPortalBase* xdpBase = options_.xdp_base();
  if (auto_close_connection_ || xdpBase->IsConnectionStreamingOnWeb(id_)) {
    xdpBase->CloseConnection(id_);
  }
}

bool BaseCapturerPipeWire::Init(const DesktopCaptureOptions& options,
                                XdgDesktopPortalBase::CaptureSourceType type) {
  options_ = options;
  type_ = type;

  XdgDesktopPortalBase::CaptureSourceType requestedType_ =
      XdgDesktopPortalBase::CaptureSourceType::kAny;

  id_ = options_.request_id();

  // We need some ID to be able to identify our capturer
  if (!id_) {
    id_ = g_random_int_range(0, G_MAXINT);
    auto_close_connection_ = true;
    requestedType_ = type;
  }

  webrtc::XdgDesktopPortalBase* xdpBase = options_.xdp_base();

  if (xdpBase->IsConnectionInitialized(id_.value())) {
    // Because capturers created for the preview dialog (Chrome, Firefox) will
    // be created simultaneously and because of that the connection cannot be
    // initialized yet, we can safely assume this a capturer created in the
    // final state to show the content on the web page itself Note: this will
    // have no effect on clients not using our specific API in
    //       DesktopCaptureOptions
    xdpBase->SetConnectionStreamingOnWeb(id_.value());
    portal_initialized_ = true;
    return true;
  }

  auto lambda = [=](bool result) { portal_initialized_ = result; };

  rtc::Callback1<void, bool> cb = lambda;
  xdpBase->InitPortal(cb, requestedType_, id_.value());

  return true;
}

void BaseCapturerPipeWire::Start(Callback* callback) {
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);

  callback_ = callback;
}

void BaseCapturerPipeWire::CaptureFrame() {
  if (!portal_initialized_) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  webrtc::XdgDesktopPortalBase* xdpBase = options_.xdp_base();
  if (type_ != XdgDesktopPortalBase::CaptureSourceType::kAny &&
      xdpBase->GetConnectionData(id_)->capture_source_type_ != type_ &&
      xdpBase->GetConnectionData(id_)->capture_source_type_ !=
          XdgDesktopPortalBase::CaptureSourceType::kAny) {
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  const webrtc::PipeWireBase* pwBase = xdpBase->GetPipeWireBase(id_);

  if (!pwBase) {
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  if (!pwBase->Frame()) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  const DesktopSize frame_size = pwBase->FrameSize();

  std::unique_ptr<DesktopFrame> result(new BasicDesktopFrame(frame_size));
  result->CopyPixelsFrom(
      pwBase->Frame(), (frame_size.width() * 4),  // kBytesPerPixel = 4
      DesktopRect::MakeWH(frame_size.width(), frame_size.height()));

  if (!result) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  // TODO(julien.isorce): http://crbug.com/945468. Set the icc profile on the
  // frame, see ScreenCapturerX11::CaptureFrame.

  callback_->OnCaptureResult(Result::SUCCESS, std::move(result));
}

bool BaseCapturerPipeWire::GetSourceList(SourceList* sources) {
  RTC_DCHECK(sources->size() == 0);
  // List of available screens is already presented by the xdg-desktop-portal.
  // But we have to add an empty source as the code expects it.
  sources->push_back({0});
  return true;
}

bool BaseCapturerPipeWire::SelectSource(SourceId id) {
  // Screen selection is handled by the xdg-desktop-portal.
  return true;
}

// static
std::unique_ptr<DesktopCapturer> BaseCapturerPipeWire::CreateRawScreenCapturer(
    const DesktopCaptureOptions& options) {
  if (!options.xdp_base())
    return nullptr;

  std::unique_ptr<BaseCapturerPipeWire> capturer =
      std::make_unique<BaseCapturerPipeWire>();
  if (!capturer->Init(options,
                      XdgDesktopPortalBase::CaptureSourceType::kScreen)) {
    return nullptr;
  }

  return std::move(capturer);
}

// static
std::unique_ptr<DesktopCapturer> BaseCapturerPipeWire::CreateRawWindowCapturer(
    const DesktopCaptureOptions& options) {
  if (!options.xdp_base())
    return nullptr;

  std::unique_ptr<BaseCapturerPipeWire> capturer =
      std::make_unique<BaseCapturerPipeWire>();
  if (!capturer->Init(options,
                      XdgDesktopPortalBase::CaptureSourceType::kWindow)) {
    return nullptr;
  }

  return std::move(capturer);
}

}  // namespace webrtc
