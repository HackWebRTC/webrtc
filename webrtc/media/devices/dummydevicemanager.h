/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_DEVICES_DUMMYDEVICEMANAGER_H_
#define WEBRTC_MEDIA_DEVICES_DUMMYDEVICEMANAGER_H_

#include <vector>

#include "webrtc/media/base/mediacommon.h"
#include "webrtc/media/devices/fakedevicemanager.h"

namespace cricket {

class DummyDeviceManager : public FakeDeviceManager {
 public:
  DummyDeviceManager() {
    std::vector<std::string> devices;
    devices.push_back(DeviceManagerInterface::kDefaultDeviceName);
    SetAudioInputDevices(devices);
    SetAudioOutputDevices(devices);
    SetVideoCaptureDevices(devices);
  }
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_DEVICES_DUMMYDEVICEMANAGER_H_
