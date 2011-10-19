/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "../device_info_impl.h"
#include "../video_capture_impl.h"

namespace webrtc {

namespace videocapturemodule {

class ExternalDeviceInfo : public DeviceInfoImpl {
 public:
  ExternalDeviceInfo(const WebRtc_Word32 id)
      : DeviceInfoImpl(id) {
  }
  virtual ~ExternalDeviceInfo() {}
  virtual WebRtc_UWord32 NumberOfDevices() { return 0; }
  virtual WebRtc_Word32 DisplayCaptureSettingsDialogBox(
      const WebRtc_UWord8* /*deviceUniqueIdUTF8*/,
      const WebRtc_UWord8* /*dialogTitleUTF8*/,
      void* /*parentWindow*/,
      WebRtc_UWord32 /*positionX*/,
      WebRtc_UWord32 /*positionY*/) { return -1; }
  virtual WebRtc_Word32 GetDeviceName(
      WebRtc_UWord32 deviceNumber,
      WebRtc_UWord8* deviceNameUTF8,
      WebRtc_UWord32 deviceNameLength,
      WebRtc_UWord8* deviceUniqueIdUTF8,
      WebRtc_UWord32 deviceUniqueIdUTF8Length,
      WebRtc_UWord8* productUniqueIdUTF8=0,
      WebRtc_UWord32 productUniqueIdUTF8Length=0) {
    return -1;
  }
  virtual WebRtc_Word32 CreateCapabilityMap(
      const WebRtc_UWord8* deviceUniqueIdUTF8) { return 0; }
  virtual WebRtc_Word32 Init() { return 0; }
};

VideoCaptureModule::DeviceInfo* VideoCaptureImpl::CreateDeviceInfo(
    const WebRtc_Word32 id) {
  return new ExternalDeviceInfo(id);
}

}  // namespace videocapturemodule

}  // namespace webrtc
