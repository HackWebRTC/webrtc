/*
 * libjingle
 * Copyright 2011, Google Inc.
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

#include "talk/app/webrtc/webrtcdevicemanager.h"

#include <vector>

using cricket::Device;
using cricket::DeviceManager;

const int WebRtcDeviceManager::kDefaultDeviceId = -1;

WebRtcDeviceManager::WebRtcDeviceManager()
    : DeviceManager(),
      default_device_(DeviceManager::kDefaultDeviceName, kDefaultDeviceId) {
}

WebRtcDeviceManager::~WebRtcDeviceManager() {
  Terminate();
}

bool WebRtcDeviceManager::Init() {
  return true;
}

void WebRtcDeviceManager::Terminate() {
}

bool WebRtcDeviceManager::GetAudioInputDevices(
    std::vector<Device>* devs) {
  return GetDefaultDevices(devs);
}

bool WebRtcDeviceManager::GetAudioOutputDevices(
    std::vector<Device>* devs) {
  return GetDefaultDevices(devs);
}

bool WebRtcDeviceManager::GetVideoCaptureDevices(
    std::vector<Device>* devs) {
  return GetDefaultDevices(devs);
}

bool WebRtcDeviceManager::GetDefaultVideoCaptureDevice(
    Device* device) {
  *device = default_device_;
  return true;
}

bool WebRtcDeviceManager::GetDefaultDevices(
    std::vector<cricket::Device>* devs) {
  if (!devs)
    return false;
  devs->clear();
  devs->push_back(default_device_);
  return true;
}
