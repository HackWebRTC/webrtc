// libjingle
// Copyright 2004 Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. The name of the author may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef TALK_SESSION_PHONE_FAKEWEBRTCDEVICEINFO_H_
#define TALK_SESSION_PHONE_FAKEWEBRTCDEVICEINFO_H_

#include <vector>

#include "talk/base/stringutils.h"
#include "talk/media/webrtc/webrtcvideocapturer.h"

// Fake class for mocking out webrtc::VideoCaptureModule::DeviceInfo.
class FakeWebRtcDeviceInfo : public webrtc::VideoCaptureModule::DeviceInfo {
 public:
  struct Device {
    Device(const std::string& n, const std::string& i) : name(n), id(i) {}
    std::string name;
    std::string id;
    std::string product;
    std::vector<webrtc::VideoCaptureCapability> caps;
  };
  FakeWebRtcDeviceInfo() {}
  void AddDevice(const std::string& device_name, const std::string& device_id) {
    devices_.push_back(Device(device_name, device_id));
  }
  void AddCapability(const std::string& device_id,
                     const webrtc::VideoCaptureCapability& cap) {
    Device* dev = GetDeviceById(
        reinterpret_cast<const char*>(device_id.c_str()));
    if (!dev) return;
    dev->caps.push_back(cap);
  }
  virtual uint32_t NumberOfDevices() {
    return devices_.size();
  }
  virtual int32_t GetDeviceName(uint32_t device_num,
                                char* device_name,
                                uint32_t device_name_len,
                                char* device_id,
                                uint32_t device_id_len,
                                char* product_id,
                                uint32_t product_id_len) {
    Device* dev = GetDeviceByIndex(device_num);
    if (!dev) return -1;
    talk_base::strcpyn(reinterpret_cast<char*>(device_name), device_name_len,
                       dev->name.c_str());
    talk_base::strcpyn(reinterpret_cast<char*>(device_id), device_id_len,
                       dev->id.c_str());
    if (product_id) {
      talk_base::strcpyn(reinterpret_cast<char*>(product_id), product_id_len,
                         dev->product.c_str());
    }
    return 0;
  }
  virtual int32_t NumberOfCapabilities(const char* device_id) {
    Device* dev = GetDeviceById(device_id);
    if (!dev) return -1;
    return dev->caps.size();
  }
  virtual int32_t GetCapability(const char* device_id,
                                const uint32_t device_cap_num,
                                webrtc::VideoCaptureCapability& cap) {
    Device* dev = GetDeviceById(device_id);
    if (!dev) return -1;
    if (device_cap_num >= dev->caps.size()) return -1;
    cap = dev->caps[device_cap_num];
    return 0;
  }
  virtual int32_t GetOrientation(const char* device_id,
                                 webrtc::VideoCaptureRotation& rotation) {
    return -1;  // not implemented
  }
  virtual int32_t GetBestMatchedCapability(
      const char* device_id,
      const webrtc::VideoCaptureCapability& requested,
      webrtc::VideoCaptureCapability& resulting) {
    return -1;  // not implemented
  }
  virtual int32_t DisplayCaptureSettingsDialogBox(
      const char* device_id, const char* dialog_title,
      void* parent, uint32_t x, uint32_t y) {
    return -1;  // not implemented
  }

  Device* GetDeviceByIndex(size_t num) {
    return (num < devices_.size()) ? &devices_[num] : NULL;
  }
  Device* GetDeviceById(const char* device_id) {
    for (size_t i = 0; i < devices_.size(); ++i) {
      if (devices_[i].id == reinterpret_cast<const char*>(device_id)) {
        return &devices_[i];
      }
    }
    return NULL;
  }

 private:
  std::vector<Device> devices_;
};

#endif  // TALK_SESSION_PHONE_FAKEWEBRTCDEVICEINFO_H_
