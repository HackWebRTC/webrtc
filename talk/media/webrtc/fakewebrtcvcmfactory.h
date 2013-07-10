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

#ifndef TALK_SESSION_PHONE_FAKEWEBRTCVCMFACTORY_H_
#define TALK_SESSION_PHONE_FAKEWEBRTCVCMFACTORY_H_

#include <vector>

#include "talk/media/webrtc/fakewebrtcvideocapturemodule.h"
#include "talk/media/webrtc/webrtcvideocapturer.h"

// Factory class to allow the fakes above to be injected into
// WebRtcVideoCapturer.
class FakeWebRtcVcmFactory : public cricket::WebRtcVcmFactoryInterface {
 public:
  virtual webrtc::VideoCaptureModule* Create(int module_id,
                                             const char* device_id) {
    if (!device_info.GetDeviceById(device_id)) return NULL;
    FakeWebRtcVideoCaptureModule* module =
        new FakeWebRtcVideoCaptureModule(this, module_id);
    modules.push_back(module);
    return module;
  }
  virtual webrtc::VideoCaptureModule::DeviceInfo* CreateDeviceInfo(int id) {
    return &device_info;
  }
  virtual void DestroyDeviceInfo(webrtc::VideoCaptureModule::DeviceInfo* info) {
  }
  void OnDestroyed(webrtc::VideoCaptureModule* module) {
    std::remove(modules.begin(), modules.end(), module);
  }
  FakeWebRtcDeviceInfo device_info;
  std::vector<FakeWebRtcVideoCaptureModule*> modules;
};

FakeWebRtcVideoCaptureModule::~FakeWebRtcVideoCaptureModule() {
  if (factory_)
    factory_->OnDestroyed(this);
}

#endif  // TALK_SESSION_PHONE_FAKEWEBRTCVCMFACTORY_H_
