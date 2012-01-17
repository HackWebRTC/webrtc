/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_capture_impl.h"

#include "system_wrappers/interface/trace.h"
#include "video_engine/include/vie_errors.h"
#include "video_engine/vie_capturer.h"
#include "video_engine/vie_channel.h"
#include "video_engine/vie_channel_manager.h"
#include "video_engine/vie_defines.h"
#include "video_engine/vie_encoder.h"
#include "video_engine/vie_impl.h"
#include "video_engine/vie_input_manager.h"

namespace webrtc {

ViECapture* ViECapture::GetInterface(VideoEngine* video_engine) {
#ifdef WEBRTC_VIDEO_ENGINE_CAPTURE_API
  if (!video_engine) {
    return NULL;
  }
  VideoEngineImpl* vie_impl = reinterpret_cast<VideoEngineImpl*>(video_engine);
  ViECaptureImpl* vie_capture_impl = vie_impl;
  // Increase ref count.
  (*vie_capture_impl)++;
  return vie_capture_impl;
#else
  return NULL;
#endif
}

int ViECaptureImpl::Release() {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, instance_id_,
               "ViECapture::Release()");
  // Decrease ref count
  (*this)--;

  WebRtc_Word32 ref_count = GetCount();
  if (ref_count < 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, instance_id_,
                 "ViECapture release too many times");
    SetLastError(kViEAPIDoesNotExist);
    return -1;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, instance_id_,
               "ViECapture reference count: %d", ref_count);
  return ref_count;
}

ViECaptureImpl::ViECaptureImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, instance_id_,
               "ViECaptureImpl::ViECaptureImpl() Ctor");
}

ViECaptureImpl::~ViECaptureImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, instance_id_,
               "ViECaptureImpl::~ViECaptureImpl() Dtor");
}

int ViECaptureImpl::NumberOfCaptureDevices() {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_), "%s",
               __FUNCTION__);
  if (!Initialized()) {
    SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 instance_id_);
    return -1;
  }
  return  input_manager_.NumberOfCaptureDevices();
}


int ViECaptureImpl::GetCaptureDevice(unsigned int list_number,
                                     char* device_nameUTF8,
                                     unsigned int device_nameUTF8Length,
                                     char* unique_idUTF8,
                                     unsigned int unique_idUTF8Length) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s(list_number: %d)", __FUNCTION__, list_number);
  if (!Initialized()) {
    SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 instance_id_);
    return -1;
  }
  return input_manager_.GetDeviceName(
      list_number,
      reinterpret_cast<WebRtc_UWord8*>(device_nameUTF8), device_nameUTF8Length,
      reinterpret_cast<WebRtc_UWord8*>(unique_idUTF8), unique_idUTF8Length);
}

int ViECaptureImpl::AllocateCaptureDevice(
  const char* unique_idUTF8,
  const unsigned int unique_idUTF8Length,
  int& capture_id) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s(unique_idUTF8: %s)", __FUNCTION__, unique_idUTF8);
  if (!Initialized()) {
    SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 instance_id_);
    return -1;
  }
  const WebRtc_Word32 result = input_manager_.CreateCaptureDevice(
      reinterpret_cast<const WebRtc_UWord8*>(unique_idUTF8),
      static_cast<const WebRtc_UWord32>(unique_idUTF8Length), capture_id);
  if (result != 0) {
    SetLastError(result);
    return -1;
  }
  return 0;
}

int ViECaptureImpl::AllocateExternalCaptureDevice(
  int& capture_id, ViEExternalCapture*& external_capture) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_), "%s",
               __FUNCTION__);

  if (!Initialized()) {
    SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 instance_id_);
    return -1;
  }
  const WebRtc_Word32 result = input_manager_.CreateExternalCaptureDevice(
      external_capture, capture_id);

  if (result != 0) {
    SetLastError(result);
    return -1;
  }
  return 0;
}

int ViECaptureImpl::AllocateCaptureDevice(VideoCaptureModule& capture_module,
                                          int& capture_id) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_), "%s",
               __FUNCTION__);

  if (!Initialized()) {
    SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 instance_id_);
    return -1;
  }
  const WebRtc_Word32 result = input_manager_.CreateCaptureDevice(
      capture_module, capture_id);
  if (result != 0) {
    SetLastError(result);
    return -1;
  }
  return 0;
}


int ViECaptureImpl::ReleaseCaptureDevice(const int capture_id) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s(capture_id: %d)", __FUNCTION__, capture_id);
  {
    ViEInputManagerScoped is(input_manager_);
    ViECapturer* vie_capture = is.Capture(capture_id);
    if (!vie_capture) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                   "%s: Capture device %d doesn't exist", __FUNCTION__,
                   capture_id);
      SetLastError(kViECaptureDeviceDoesNotExist);
      return -1;
    }
  }

  // Destroy the capture device.
  return input_manager_.DestroyCaptureDevice(capture_id);
}

int ViECaptureImpl::ConnectCaptureDevice(const int capture_id,
                                         const int video_channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_, video_channel),
               "%s(capture_id: %d, video_channel: %d)", __FUNCTION__,
               capture_id, video_channel);

  ViEInputManagerScoped is(input_manager_);
  ViECapturer* vie_capture = is.Capture(capture_id);
  if (!vie_capture) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Capture device %d doesn't exist", __FUNCTION__,
                 capture_id);
    SetLastError(kViECaptureDeviceDoesNotExist);
    return -1;
  }

  ViEChannelManagerScoped cs(channel_manager_);
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__,
                 video_channel);
    SetLastError(kViECaptureDeviceInvalidChannelId);
    return -1;
  }
  //  Check if the encoder already has a connected frame provider
  if (is.FrameProvider(vie_encoder) != NULL) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Channel %d already connected to a capture device.",
                 __FUNCTION__, video_channel);
    SetLastError(kViECaptureDeviceAlreadyConnected);
    return -1;
  }
  VideoCodec codec;
  bool use_hardware_encoder = false;
  if (vie_encoder->GetEncoder(codec) == 0) {
    // Try to provide the encoder with pre-encoded frames if possible.
    if (vie_capture->PreEncodeToViEEncoder(codec, *vie_encoder,
                                           video_channel) == 0) {
      use_hardware_encoder = true;
    }
  }
  // If we don't use the camera as hardware encoder, we register the vie_encoder
  // for callbacks.
  if (!use_hardware_encoder &&
      vie_capture->RegisterFrameCallback(video_channel, vie_encoder) != 0) {
    SetLastError(kViECaptureDeviceUnknownError);
    return -1;
  }
  return 0;
}


int ViECaptureImpl::DisconnectCaptureDevice(const int video_channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_, video_channel),
               "%s(video_channel: %d)", __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(channel_manager_);
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                 "%s: Channel %d doesn't exist", __FUNCTION__,
                 video_channel);
    SetLastError(kViECaptureDeviceInvalidChannelId);
    return -1;
  }

  ViEInputManagerScoped is(input_manager_);
  ViEFrameProviderBase* frame_provider = is.FrameProvider(vie_encoder);
  if (!frame_provider) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(instance_id_),
                 "%s: No capture device connected to channel %d",
                 __FUNCTION__, video_channel);
    SetLastError(kViECaptureDeviceNotConnected);
    return -1;
  }
  if (frame_provider->Id() < kViECaptureIdBase ||
      frame_provider->Id() > kViECaptureIdMax) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(instance_id_),
                 "%s: No capture device connected to channel %d",
                 __FUNCTION__, video_channel);
    SetLastError(kViECaptureDeviceNotConnected);
    return -1;
  }

  if (frame_provider->DeregisterFrameCallback(vie_encoder) != 0) {
    SetLastError(kViECaptureDeviceUnknownError);
    return -1;
  }

  return 0;
}

int ViECaptureImpl::StartCapture(const int capture_id,
                                 const CaptureCapability& capture_capability) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s(capture_id: %d)", __FUNCTION__, capture_id);

  ViEInputManagerScoped is(input_manager_);
  ViECapturer* vie_capture = is.Capture(capture_id);
  if (!vie_capture) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, capture_id),
                 "%s: Capture device %d doesn't exist", __FUNCTION__,
                 capture_id);
    SetLastError(kViECaptureDeviceDoesNotExist);
    return -1;
  }
  if (vie_capture->Started()) {
    SetLastError(kViECaptureDeviceAlreadyStarted);
    return -1;
  }
  if (vie_capture->Start(capture_capability) != 0) {
    SetLastError(kViECaptureDeviceUnknownError);
    return -1;
  }
  return 0;
}

int ViECaptureImpl::StopCapture(const int capture_id) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s(capture_id: %d)", __FUNCTION__, capture_id);

  ViEInputManagerScoped is(input_manager_);
  ViECapturer* vie_capture = is.Capture(capture_id);
  if (!vie_capture) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, capture_id),
                 "%s: Capture device %d doesn't exist", __FUNCTION__,
                 capture_id);
    SetLastError(kViECaptureDeviceDoesNotExist);
    return -1;
  }
  if (!vie_capture->Started()) {
    SetLastError(kViECaptureDeviceNotStarted);
    return -1;
  }
  if (vie_capture->Stop() != 0) {
    SetLastError(kViECaptureDeviceUnknownError);
    return -1;
  }

  return 0;
}

int ViECaptureImpl::SetRotateCapturedFrames(
    const int capture_id,
    const RotateCapturedFrame rotation) {
  int i_rotation = -1;
  switch (rotation) {
    case RotateCapturedFrame_0:
      i_rotation = 0;
      break;
    case RotateCapturedFrame_90:
      i_rotation = 90;
      break;
    case RotateCapturedFrame_180:
      i_rotation = 180;
      break;
    case RotateCapturedFrame_270:
      i_rotation = 270;
      break;
  }
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s(rotation: %d)", __FUNCTION__, i_rotation);

  ViEInputManagerScoped is(input_manager_);
  ViECapturer* vie_capture = is.Capture(capture_id);
  if (!vie_capture) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, capture_id),
                 "%s: Capture device %d doesn't exist", __FUNCTION__,
                 capture_id);
    SetLastError(kViECaptureDeviceDoesNotExist);
    return -1;
  }
  if (vie_capture->SetRotateCapturedFrames(rotation) != 0) {
    SetLastError(kViECaptureDeviceUnknownError);
    return -1;
  }
  return 0;
}

int ViECaptureImpl::SetCaptureDelay(const int capture_id,
                                    const unsigned int capture_delay_ms) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s(capture_id: %d, capture_delay_ms %u)", __FUNCTION__,
               capture_id, capture_delay_ms);

  ViEInputManagerScoped is(input_manager_);
  ViECapturer* vie_capture = is.Capture(capture_id);
  if (!vie_capture) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, capture_id),
                 "%s: Capture device %d doesn't exist", __FUNCTION__,
                 capture_id);
    SetLastError(kViECaptureDeviceDoesNotExist);
    return -1;
  }

  if (vie_capture->SetCaptureDelay(capture_delay_ms) != 0) {
    SetLastError(kViECaptureDeviceUnknownError);
    return -1;
  }
  return 0;
}

int ViECaptureImpl::NumberOfCapabilities(
    const char* unique_idUTF8,
    const unsigned int unique_idUTF8Length) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s(capture_device_name: %s)", __FUNCTION__, unique_idUTF8);

#if defined(WEBRTC_MAC_INTEL)
  // TODO(mflodman) Move to capture module!
  // QTKit framework handles all capabilities and capture settings
  // automatically (mandatory).
  // Thus this function cannot be supported on the Mac platform.
  SetLastError(kViECaptureDeviceMacQtkitNotSupported);
  WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
               "%s This API is not supported on Mac OS", __FUNCTION__,
               instance_id_);
  return -1;
#endif

  if (!Initialized()) {
    SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 instance_id_);
    return -1;
  }
  return input_manager_.NumberOfCaptureCapabilities(
      reinterpret_cast<const WebRtc_UWord8*>(unique_idUTF8));
}


int ViECaptureImpl::GetCaptureCapability(const char* unique_idUTF8,
                                         const unsigned int unique_idUTF8Length,
                                         const unsigned int capability_number,
                                         CaptureCapability& capability) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s(capture_device_name: %s)", __FUNCTION__, unique_idUTF8);

#if defined(WEBRTC_MAC_INTEL)
  // TODO(mflodman) Move to capture module!
  // QTKit framework handles all capabilities and capture settings
  // automatically (mandatory).
  // Thus this function cannot be supported on the Mac platform.
  SetLastError(kViECaptureDeviceMacQtkitNotSupported);
  WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
               "%s This API is not supported on Mac OS", __FUNCTION__,
               instance_id_);
  return -1;
#endif
  if (!Initialized()) {
    SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 instance_id_);
    return -1;
  }
  if (input_manager_.GetCaptureCapability(
      reinterpret_cast<const WebRtc_UWord8*>(unique_idUTF8),
      capability_number, capability) != 0) {
    SetLastError(kViECaptureDeviceUnknownError);
    return -1;
  }
  return 0;
}

int ViECaptureImpl::ShowCaptureSettingsDialogBox(
    const char* unique_idUTF8,
    const unsigned int unique_idUTF8Length,
    const char* dialog_title,
    void* parent_window,
    const unsigned int x,
    const unsigned int y) {
#if defined(WEBRTC_MAC_INTEL)
  // TODO(mflodman) Move to capture module
  // QTKit framework handles all capabilities and capture settings
  // automatically (mandatory).
  // Thus this function cannot be supported on the Mac platform.
  SetLastError(kViECaptureDeviceMacQtkitNotSupported);
  WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
               "%s This API is not supported on Mac OS", __FUNCTION__,
               instance_id_);
  return -1;
#endif
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s capture_id (capture_device_name: %s)", __FUNCTION__,
               unique_idUTF8);

  return input_manager_.DisplayCaptureSettingsDialogBox(
           reinterpret_cast<const WebRtc_UWord8*>(unique_idUTF8),
           reinterpret_cast<const WebRtc_UWord8*>(dialog_title),
           parent_window, x, y);
}

int ViECaptureImpl::GetOrientation(const char* unique_idUTF8,
                                   RotateCapturedFrame& orientation) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s (capture_device_name: %s)", __FUNCTION__, unique_idUTF8);

  if (!Initialized()) {
    SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 instance_id_);
    return -1;
  }
  if (input_manager_.GetOrientation(
      reinterpret_cast<const WebRtc_UWord8*>(unique_idUTF8),
      orientation) != 0) {
    SetLastError(kViECaptureDeviceUnknownError);
    return -1;
  }
  return 0;
}


int ViECaptureImpl::EnableBrightnessAlarm(const int capture_id,
                                          const bool enable) {
  ViEInputManagerScoped is(input_manager_);
  ViECapturer* vie_capture = is.Capture(capture_id);
  if (!vie_capture) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, capture_id),
                 "%s: Capture device %d doesn't exist", __FUNCTION__,
                 capture_id);
    SetLastError(kViECaptureDeviceDoesNotExist);
    return -1;
  }
  if (vie_capture->EnableBrightnessAlarm(enable) != 0) {
    SetLastError(kViECaptureDeviceUnknownError);
    return -1;
  }
  return 0;
}

int ViECaptureImpl::RegisterObserver(const int capture_id,
                                     ViECaptureObserver& observer) {
  ViEInputManagerScoped is(input_manager_);
  ViECapturer* vie_capture = is.Capture(capture_id);
  if (!vie_capture) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, capture_id),
                 "%s: Capture device %d doesn't exist", __FUNCTION__,
                 capture_id);
    SetLastError(kViECaptureDeviceDoesNotExist);
    return -1;
  }
  if (vie_capture->IsObserverRegistered()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, capture_id),
                 "%s: Observer already registered", __FUNCTION__);
    SetLastError(kViECaptureObserverAlreadyRegistered);
    return -1;
  }
  if (vie_capture->RegisterObserver(observer) != 0) {
    SetLastError(kViECaptureDeviceUnknownError);
    return -1;
  }
  return 0;
}

int ViECaptureImpl::DeregisterObserver(const int capture_id) {
  ViEInputManagerScoped is(input_manager_);
  ViECapturer* vie_capture = is.Capture(capture_id);
  if (!vie_capture) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, capture_id),
                 "%s: Capture device %d doesn't exist", __FUNCTION__,
                 capture_id);
    SetLastError(kViECaptureDeviceDoesNotExist);
    return -1;
  }
  if (!vie_capture->IsObserverRegistered()) {
    SetLastError(kViECaptureDeviceObserverNotRegistered);
    return -1;
  }

  if (vie_capture->DeRegisterObserver() != 0) {
    SetLastError(kViECaptureDeviceUnknownError);
    return -1;
  }
  return 0;
}

}  // namespace webrtc
