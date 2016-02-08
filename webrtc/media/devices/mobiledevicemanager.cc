/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/arraysize.h"
#include "webrtc/media/devices/devicemanager.h"
#include "webrtc/modules/video_capture/video_capture_factory.h"

namespace cricket {

class MobileDeviceManager : public DeviceManager {
 public:
  MobileDeviceManager();
  virtual ~MobileDeviceManager();
  virtual bool GetVideoCaptureDevices(std::vector<Device>* devs);
};

MobileDeviceManager::MobileDeviceManager() {
  // We don't expect available devices to change on Android/iOS, so use a
  // do-nothing watcher.
  set_watcher(new DeviceWatcher(this));
}

MobileDeviceManager::~MobileDeviceManager() {}

bool MobileDeviceManager::GetVideoCaptureDevices(std::vector<Device>* devs) {
  devs->clear();
  rtc::scoped_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
      webrtc::VideoCaptureFactory::CreateDeviceInfo(0));
  if (!info)
    return false;

  uint32_t num_cams = info->NumberOfDevices();
  char id[256];
  char name[256];
  for (uint32_t i = 0; i < num_cams; ++i) {
    if (info->GetDeviceName(i, name, arraysize(name), id, arraysize(id)))
      continue;
    devs->push_back(Device(name, id));
  }
  return true;
}

DeviceManagerInterface* DeviceManagerFactory::Create() {
  return new MobileDeviceManager();
}

bool GetUsbId(const Device& device, std::string* usb_id) { return false; }

bool GetUsbVersion(const Device& device, std::string* usb_version) {
  return false;
}

}  // namespace cricket
