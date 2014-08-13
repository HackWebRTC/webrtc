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

#include "talk/media/base/device.h"
#include "talk/media/base/screencastid.h"
#include "talk/media/base/videocapturerfactory.h"
#include "talk/media/base/videocommon.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/window.h"

namespace rtc {

class DesktopDescription;
class WindowDescription;
class WindowPicker;

}
namespace cricket {

class VideoCapturer;

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

  // If the device manager needs to create video capturers, here is
  // how to control which video capturers are created.  These take
  // ownership of the factories.
  virtual void SetVideoDeviceCapturerFactory(
      VideoDeviceCapturerFactory* video_device_capturer_factory) = 0;
  virtual void SetScreenCapturerFactory(
      ScreenCapturerFactory* screen_capturer_factory) = 0;

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
      std::vector<rtc::WindowDescription>* descriptions) = 0;
  virtual bool GetDesktops(
      std::vector<rtc::DesktopDescription>* descriptions) = 0;
  virtual VideoCapturer* CreateScreenCapturer(
      const ScreencastId& screenid) const = 0;

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

  virtual void SetVideoDeviceCapturerFactory(
      VideoDeviceCapturerFactory* video_device_capturer_factory) {
    video_device_capturer_factory_.reset(video_device_capturer_factory);
  }
  virtual void SetScreenCapturerFactory(
      ScreenCapturerFactory* screen_capturer_factory) {
    screen_capturer_factory_.reset(screen_capturer_factory);
  }


  virtual void SetVideoCaptureDeviceMaxFormat(const std::string& usb_id,
                                              const VideoFormat& max_format);
  virtual void ClearVideoCaptureDeviceMaxFormat(const std::string& usb_id);

  // TODO(pthatcher): Rename to CreateVideoDeviceCapturer.
  virtual VideoCapturer* CreateVideoCapturer(const Device& device) const;

  virtual bool GetWindows(
      std::vector<rtc::WindowDescription>* descriptions);
  virtual bool GetDesktops(
      std::vector<rtc::DesktopDescription>* descriptions);
  virtual VideoCapturer* CreateScreenCapturer(
      const ScreencastId& screenid) const;

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
  bool GetFakeVideoCaptureDevice(const std::string& name, Device* out) const;
  VideoCapturer* MaybeConstructFakeVideoCapturer(const Device& device) const;

  bool initialized_;
  rtc::scoped_ptr<
    VideoDeviceCapturerFactory> video_device_capturer_factory_;
  rtc::scoped_ptr<
    ScreenCapturerFactory> screen_capturer_factory_;
  std::map<std::string, VideoFormat> max_formats_;
  rtc::scoped_ptr<DeviceWatcher> watcher_;
  rtc::scoped_ptr<rtc::WindowPicker> window_picker_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_DEVICES_DEVICEMANAGER_H_
