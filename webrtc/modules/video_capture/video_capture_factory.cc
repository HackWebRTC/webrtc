/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_capture/include/video_capture_factory.h"

#include "webrtc/modules/video_capture/video_capture_impl.h"

namespace webrtc
{

VideoCaptureModule* VideoCaptureFactory::Create(const int32_t id,
    const char* deviceUniqueIdUTF8) {
  return videocapturemodule::VideoCaptureImpl::Create(id, deviceUniqueIdUTF8);
}

VideoCaptureModule* VideoCaptureFactory::Create(const int32_t id,
    VideoCaptureExternal*& externalCapture) {
  return videocapturemodule::VideoCaptureImpl::Create(id, externalCapture);
}

VideoCaptureModule::DeviceInfo* VideoCaptureFactory::CreateDeviceInfo(
    const int32_t id) {
  return videocapturemodule::VideoCaptureImpl::CreateDeviceInfo(id);
}

// TODO(sjlee): land https://webrtc-codereview.appspot.com/1641004/
#ifdef WEBRTC_IOS
namespace videocapturemodule {
VideoCaptureModule* VideoCaptureImpl::Create(int32_t, const char*) {
  return NULL;
}

VideoCaptureModule::DeviceInfo* VideoCaptureImpl::CreateDeviceInfo(int32_t) {
  return NULL;
}
} // namespace videocaptureimpl
#endif

}  // namespace webrtc
