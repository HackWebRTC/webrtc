/*
 * libjingle
 * Copyright 2004--2008, Google Inc.
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

#ifndef TALK_SESSION_PHONE_DEVICEMANAGER_H_
#define TALK_SESSION_PHONE_DEVICEMANAGER_H_

#include <string>
#include <vector>

#include "talk/base/sigslot.h"
#include "talk/base/stringencode.h"
#ifdef LINUX_SOUND_USED
#include "talk/sound/soundsystemfactory.h"
#endif

namespace cricket {

class DeviceWatcher;

// Used to represent an audio or video capture or render device.
class Device {
 public:
  Device() {}
  Device(const std::string& first, int second)
      : name(first),
        id(talk_base::ToString(second)) {
  }
  Device(const std::string& first, const std::string& second)
      : name(first), id(second) {}

  std::string name;
  std::string id;
};

// DeviceManager manages the audio and video devices on the system.
// Methods are virtual to allow for easy stubbing/mocking in tests.
class DeviceManager {
 public:
  DeviceManager();
  virtual ~DeviceManager();

  // Initialization
  virtual bool Init();
  virtual void Terminate();
  bool initialized() const { return initialized_; }

  // Capabilities
  virtual int GetCapabilities();

  // Device enumeration
  virtual bool GetAudioInputDevices(std::vector<Device>* devices);
  virtual bool GetAudioOutputDevices(std::vector<Device>* devices);

  bool GetAudioInputDevice(const std::string& name, Device* out);
  bool GetAudioOutputDevice(const std::string& name, Device* out);

  virtual bool GetVideoCaptureDevices(std::vector<Device>* devs);
  bool GetVideoCaptureDevice(const std::string& name, Device* out);

  sigslot::signal0<> SignalDevicesChange;

  void OnDevicesChange() { SignalDevicesChange(); }

  static const std::string kDefaultDeviceName;

 protected:
  virtual bool GetAudioDevice(bool is_input, const std::string& name,
                              Device* out);
  virtual bool GetDefaultVideoCaptureDevice(Device* device);

 private:
  bool GetAudioDevicesByPlatform(bool input, std::vector<Device>* devs);

  bool initialized_;
#ifdef WIN32
  bool need_couninitialize_;
#endif
  DeviceWatcher* watcher_;
#ifdef LINUX_SOUND_USED
  SoundSystemHandle sound_system_;
#endif
};

}  // namespace cricket

#endif  // TALK_SESSION_PHONE_DEVICEMANAGER_H_
