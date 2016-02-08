/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_DEVICES_LINUXDEVICEMANAGER_H_
#define WEBRTC_MEDIA_DEVICES_LINUXDEVICEMANAGER_H_

#include <string>
#include <vector>

#include "webrtc/base/sigslot.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/media/devices/devicemanager.h"
#include "webrtc/sound/soundsystemfactory.h"

namespace cricket {

class LinuxDeviceManager : public DeviceManager {
 public:
  LinuxDeviceManager();
  virtual ~LinuxDeviceManager();

  virtual bool GetVideoCaptureDevices(std::vector<Device>* devs);

 private:
  virtual bool GetAudioDevices(bool input, std::vector<Device>* devs);
  rtc::SoundSystemHandle sound_system_;
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_DEVICES_LINUXDEVICEMANAGER_H_
