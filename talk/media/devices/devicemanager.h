/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#ifndef TALK_MEDIA_DEVICES_DEVICEMANAGER_H_
#define TALK_MEDIA_DEVICES_DEVICEMANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "talk/base/scoped_ptr.h"
#include "talk/base/sigslot.h"
#include "talk/base/stringencode.h"
#include "talk/base/window.h"
#include "talk/media/base/videocommon.h"

namespace talk_base {

class DesktopDescription;
class WindowDescription;
class WindowPicker;

}
namespace cricket {

class VideoCapturer;

// Used to represent an audio or video capture or render device.
struct Device {
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

class VideoCapturerFactory {
 public:
  VideoCapturerFactory() {}
  virtual ~VideoCapturerFactory() {}

  virtual VideoCapturer* Create(const Device& device) = 0;
};

// DeviceManagerInterface - interface to manage the audio and
// video devices on the system.
class DeviceManagerInterface {
 public:
  virtual ~DeviceManagerInterface() { }

  // Initialization
  virtual bool Init() = 0;
  virtual void Terminate() = 0;

  // Capabilities
  virtual int GetCapabilities() = 0;

  // Device enumeration
  virtual bool GetAudioInputDevices(std::vector<Device>* devices) = 0;
  virtual bool GetAudioOutputDevices(std::vector<Device>* devices) = 0;

  virtual bool GetAudioInputDevice(const std::string& name, Device* out) = 0;
  virtual bool GetAudioOutputDevice(const std::string& name, Device* out) = 0;

  virtual bool GetVideoCaptureDevices(std::vector<Device>* devs) = 0;
  virtual bool GetVideoCaptureDevice(const std::string& name, Device* out) = 0;

  // Caps the capture format according to max format for capturers created
  // by CreateVideoCapturer(). See ConstrainSupportedFormats() in
  // videocapturer.h for more detail.
  // Note that once a VideoCapturer has been created, calling this API will
  // not affect it.
  virtual void SetVideoCaptureDeviceMaxFormat(
      const std::string& usb_id,
      const VideoFormat& max_format) = 0;
  virtual void ClearVideoCaptureDeviceMaxFormat(const std::string& usb_id) = 0;

  // Device creation
  virtual VideoCapturer* CreateVideoCapturer(const Device& device) const = 0;

  virtual bool GetWindows(
      std::vector<talk_base::WindowDescription>* descriptions) = 0;
  virtual VideoCapturer* CreateWindowCapturer(talk_base::WindowId window) = 0;

  virtual bool GetDesktops(
      std::vector<talk_base::DesktopDescription>* descriptions) = 0;
  virtual VideoCapturer* CreateDesktopCapturer(
      talk_base::DesktopId desktop) = 0;

  sigslot::signal0<> SignalDevicesChange;

  static const char kDefaultDeviceName[];
};

class DeviceWatcher {
 public:
  explicit DeviceWatcher(DeviceManagerInterface* dm) {}
  virtual ~DeviceWatcher() {}
  virtual bool Start() { return true; }
  virtual void Stop() {}
};

class DeviceManagerFactory {
 public:
  static DeviceManagerInterface* Create();

 private:
  DeviceManagerFactory() {}
};

class DeviceManager : public DeviceManagerInterface {
 public:
  DeviceManager();
  virtual ~DeviceManager();

  void set_device_video_capturer_factory(
      VideoCapturerFactory* device_video_capturer_factory) {
    device_video_capturer_factory_.reset(device_video_capturer_factory);
  }

  // Initialization
  virtual bool Init();
  virtual void Terminate();

  // Capabilities
  virtual int GetCapabilities();

  // Device enumeration
  virtual bool GetAudioInputDevices(std::vector<Device>* devices);
  virtual bool GetAudioOutputDevices(std::vector<Device>* devices);

  virtual bool GetAudioInputDevice(const std::string& name, Device* out);
  virtual bool GetAudioOutputDevice(const std::string& name, Device* out);

  virtual bool GetVideoCaptureDevices(std::vector<Device>* devs);
  virtual bool GetVideoCaptureDevice(const std::string& name, Device* out);

  virtual void SetVideoCaptureDeviceMaxFormat(const std::string& usb_id,
                                              const VideoFormat& max_format);
  virtual void ClearVideoCaptureDeviceMaxFormat(const std::string& usb_id);

  virtual VideoCapturer* CreateVideoCapturer(const Device& device) const;

  virtual bool GetWindows(
      std::vector<talk_base::WindowDescription>* descriptions);
  virtual VideoCapturer* CreateWindowCapturer(talk_base::WindowId window);

  virtual bool GetDesktops(
      std::vector<talk_base::DesktopDescription>* descriptions);
  virtual VideoCapturer* CreateDesktopCapturer(talk_base::DesktopId desktop);

  // The exclusion_list MUST be a NULL terminated list.
  static bool FilterDevices(std::vector<Device>* devices,
      const char* const exclusion_list[]);
  bool initialized() const { return initialized_; }

 protected:
  virtual bool GetAudioDevices(bool input, std::vector<Device>* devs);
  virtual bool GetAudioDevice(bool is_input, const std::string& name,
                              Device* out);
  virtual bool GetDefaultVideoCaptureDevice(Device* device);
  bool IsInWhitelist(const std::string& key, VideoFormat* video_format) const;
  virtual bool GetMaxFormat(const Device& device,
                            VideoFormat* video_format) const;

  void set_initialized(bool initialized) { initialized_ = initialized; }

  void set_watcher(DeviceWatcher* watcher) { watcher_.reset(watcher); }
  DeviceWatcher* watcher() { return watcher_.get(); }

 private:
  // The exclusion_list MUST be a NULL terminated list.
  static bool ShouldDeviceBeIgnored(const std::string& device_name,
      const char* const exclusion_list[]);

  bool initialized_;
  talk_base::scoped_ptr<VideoCapturerFactory> device_video_capturer_factory_;
  std::map<std::string, VideoFormat> max_formats_;
  talk_base::scoped_ptr<DeviceWatcher> watcher_;
  talk_base::scoped_ptr<talk_base::WindowPicker> window_picker_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_DEVICES_DEVICEMANAGER_H_
