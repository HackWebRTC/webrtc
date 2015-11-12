/*
 * libjingle
 * Copyright 2013 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/media/devices/devicemanager.h"
#include "webrtc/base/arraysize.h"
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
