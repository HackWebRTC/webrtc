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

#include "talk/media/devices/win32devicemanager.h"

#include <atlbase.h>
#include <dbt.h>
#include <strmif.h>  // must come before ks.h
#include <ks.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <mmsystem.h>
#include <functiondiscoverykeys_devpkey.h>
#include <uuids.h>

// PKEY_AudioEndpoint_GUID isn't included in uuid.lib and we don't want
// to define INITGUID in order to define all the uuids in this object file
// as it will conflict with uuid.lib (multiply defined symbols).
// So our workaround is to define this one missing symbol here manually.
// See: https://code.google.com/p/webrtc/issues/detail?id=3996
EXTERN_C const PROPERTYKEY PKEY_AudioEndpoint_GUID = { {
  0x1da5d803, 0xd492, 0x4edd, {
    0x8c, 0x23, 0xe0, 0xc0, 0xff, 0xee, 0x7f, 0x0e
  } }, 4
};

#include "webrtc/base/arraysize.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/win32.h"  // ToUtf8
#include "webrtc/base/win32window.h"
#include "talk/media/base/mediacommon.h"
#ifdef HAVE_LOGITECH_HEADERS
#include "third_party/logitech/files/logitechquickcam.h"
#endif

namespace cricket {

DeviceManagerInterface* DeviceManagerFactory::Create() {
  return new Win32DeviceManager();
}

class Win32DeviceWatcher
    : public DeviceWatcher,
      public rtc::Win32Window {
 public:
  explicit Win32DeviceWatcher(Win32DeviceManager* dm);
  virtual ~Win32DeviceWatcher();
  virtual bool Start();
  virtual void Stop();

 private:
  HDEVNOTIFY Register(REFGUID guid);
  void Unregister(HDEVNOTIFY notify);
  virtual bool OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT& result);

  Win32DeviceManager* manager_;
  HDEVNOTIFY audio_notify_;
  HDEVNOTIFY video_notify_;
};

static const char* kFilteredAudioDevicesName[] = {
    NULL,
};
static const char* const kFilteredVideoDevicesName[] =  {
    "Asus virtual Camera",     // Bad Asus desktop virtual cam
    "Bluetooth Video",         // Bad Sony viao bluetooth sharing driver
    NULL,
};
static const wchar_t kFriendlyName[] = L"FriendlyName";
static const wchar_t kDevicePath[] = L"DevicePath";
static const char kUsbDevicePathPrefix[] = "\\\\?\\usb";
static bool GetDevices(const CLSID& catid, std::vector<Device>* out);
static bool GetCoreAudioDevices(bool input, std::vector<Device>* devs);
static bool GetWaveDevices(bool input, std::vector<Device>* devs);

Win32DeviceManager::Win32DeviceManager()
    : need_couninitialize_(false) {
  set_watcher(new Win32DeviceWatcher(this));
}

Win32DeviceManager::~Win32DeviceManager() {
  if (initialized()) {
    Terminate();
  }
}

bool Win32DeviceManager::Init() {
  if (!initialized()) {
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    need_couninitialize_ = SUCCEEDED(hr);
    if (FAILED(hr)) {
      LOG(LS_ERROR) << "CoInitialize failed, hr=" << hr;
      if (hr != RPC_E_CHANGED_MODE) {
        return false;
      }
    }
    if (!watcher()->Start()) {
      return false;
    }
    set_initialized(true);
  }
  return true;
}

void Win32DeviceManager::Terminate() {
  if (initialized()) {
    watcher()->Stop();
    if (need_couninitialize_) {
      CoUninitialize();
      need_couninitialize_ = false;
    }
    set_initialized(false);
  }
}

bool Win32DeviceManager::GetDefaultVideoCaptureDevice(Device* device) {
  bool ret = false;
  // If there are multiple capture devices, we want the first USB one.
  // This avoids issues with defaulting to virtual cameras or grabber cards.
  std::vector<Device> devices;
  ret = (GetVideoCaptureDevices(&devices) && !devices.empty());
  if (ret) {
    *device = devices[0];
    for (size_t i = 0; i < devices.size(); ++i) {
      if (strnicmp(devices[i].id.c_str(), kUsbDevicePathPrefix,
                   arraysize(kUsbDevicePathPrefix) - 1) == 0) {
        *device = devices[i];
        break;
      }
    }
  }
  return ret;
}

bool Win32DeviceManager::GetAudioDevices(bool input,
                                         std::vector<Device>* devs) {
  devs->clear();

  if (rtc::IsWindowsVistaOrLater()) {
    if (!GetCoreAudioDevices(input, devs))
      return false;
  } else {
    if (!GetWaveDevices(input, devs))
      return false;
  }
  return FilterDevices(devs, kFilteredAudioDevicesName);
}

bool Win32DeviceManager::GetVideoCaptureDevices(std::vector<Device>* devices) {
  devices->clear();
  if (!GetDevices(CLSID_VideoInputDeviceCategory, devices)) {
    return false;
  }
  return FilterDevices(devices, kFilteredVideoDevicesName);
}

bool GetDevices(const CLSID& catid, std::vector<Device>* devices) {
  HRESULT hr;

  // CComPtr is a scoped pointer that will be auto released when going
  // out of scope. CoUninitialize must not be called before the
  // release.
  CComPtr<ICreateDevEnum> sys_dev_enum;
  CComPtr<IEnumMoniker> cam_enum;
  if (FAILED(hr = sys_dev_enum.CoCreateInstance(CLSID_SystemDeviceEnum)) ||
      FAILED(hr = sys_dev_enum->CreateClassEnumerator(catid, &cam_enum, 0))) {
    LOG(LS_ERROR) << "Failed to create device enumerator, hr="  << hr;
    return false;
  }

  // Only enum devices if CreateClassEnumerator returns S_OK. If there are no
  // devices available, S_FALSE will be returned, but enumMk will be NULL.
  if (hr == S_OK) {
    CComPtr<IMoniker> mk;
    while (cam_enum->Next(1, &mk, NULL) == S_OK) {
#ifdef HAVE_LOGITECH_HEADERS
      // Initialize Logitech device if applicable
      MaybeLogitechDeviceReset(mk);
#endif
      CComPtr<IPropertyBag> bag;
      if (SUCCEEDED(mk->BindToStorage(NULL, NULL,
          __uuidof(bag), reinterpret_cast<void**>(&bag)))) {
        CComVariant name, path;
        std::string name_str, path_str;
        if (SUCCEEDED(bag->Read(kFriendlyName, &name, 0)) &&
            name.vt == VT_BSTR) {
          name_str = rtc::ToUtf8(name.bstrVal);
          // Get the device id if one exists.
          if (SUCCEEDED(bag->Read(kDevicePath, &path, 0)) &&
              path.vt == VT_BSTR) {
            path_str = rtc::ToUtf8(path.bstrVal);
          }

          devices->push_back(Device(name_str, path_str));
        }
      }
      mk = NULL;
    }
  }

  return true;
}

HRESULT GetStringProp(IPropertyStore* bag, PROPERTYKEY key, std::string* out) {
  out->clear();
  PROPVARIANT var;
  PropVariantInit(&var);

  HRESULT hr = bag->GetValue(key, &var);
  if (SUCCEEDED(hr)) {
    if (var.pwszVal)
      *out = rtc::ToUtf8(var.pwszVal);
    else
      hr = E_FAIL;
  }

  PropVariantClear(&var);
  return hr;
}

// Adapted from http://msdn.microsoft.com/en-us/library/dd370812(v=VS.85).aspx
HRESULT CricketDeviceFromImmDevice(IMMDevice* device, Device* out) {
  CComPtr<IPropertyStore> props;

  HRESULT hr = device->OpenPropertyStore(STGM_READ, &props);
  if (FAILED(hr)) {
    return hr;
  }

  // Get the endpoint's name and id.
  std::string name, guid;
  hr = GetStringProp(props, PKEY_Device_FriendlyName, &name);
  if (SUCCEEDED(hr)) {
    hr = GetStringProp(props, PKEY_AudioEndpoint_GUID, &guid);

    if (SUCCEEDED(hr)) {
      out->name = name;
      out->id = guid;
    }
  }
  return hr;
}

bool GetCoreAudioDevices(
    bool input, std::vector<Device>* devs) {
  HRESULT hr = S_OK;
  CComPtr<IMMDeviceEnumerator> enumerator;

  hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
      __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
  if (SUCCEEDED(hr)) {
    CComPtr<IMMDeviceCollection> devices;
    hr = enumerator->EnumAudioEndpoints((input ? eCapture : eRender),
                                        DEVICE_STATE_ACTIVE, &devices);
    if (SUCCEEDED(hr)) {
      unsigned int count;
      hr = devices->GetCount(&count);

      if (SUCCEEDED(hr)) {
        for (unsigned int i = 0; i < count; i++) {
          CComPtr<IMMDevice> device;

          // Get pointer to endpoint number i.
          hr = devices->Item(i, &device);
          if (FAILED(hr)) {
            break;
          }

          Device dev;
          hr = CricketDeviceFromImmDevice(device, &dev);
          if (SUCCEEDED(hr)) {
            devs->push_back(dev);
          } else {
            LOG(LS_WARNING) << "Unable to query IMM Device, skipping.  HR="
                            << hr;
            hr = S_FALSE;
          }
        }
      }
    }
  }

  if (FAILED(hr)) {
    LOG(LS_WARNING) << "GetCoreAudioDevices failed with hr " << hr;
    return false;
  }
  return true;
}

bool GetWaveDevices(bool input, std::vector<Device>* devs) {
  // Note, we don't use the System Device Enumerator interface here since it
  // adds lots of pseudo-devices to the list, such as DirectSound and Wave
  // variants of the same device.
  if (input) {
    int num_devs = waveInGetNumDevs();
    for (int i = 0; i < num_devs; ++i) {
      WAVEINCAPS caps;
      if (waveInGetDevCaps(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR &&
          caps.wChannels > 0) {
        devs->push_back(Device(rtc::ToUtf8(caps.szPname),
                               rtc::ToString(i)));
      }
    }
  } else {
    int num_devs = waveOutGetNumDevs();
    for (int i = 0; i < num_devs; ++i) {
      WAVEOUTCAPS caps;
      if (waveOutGetDevCaps(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR &&
          caps.wChannels > 0) {
        devs->push_back(Device(rtc::ToUtf8(caps.szPname), i));
      }
    }
  }
  return true;
}

Win32DeviceWatcher::Win32DeviceWatcher(Win32DeviceManager* manager)
    : DeviceWatcher(manager),
      manager_(manager),
      audio_notify_(NULL),
      video_notify_(NULL) {
}

Win32DeviceWatcher::~Win32DeviceWatcher() {
}

bool Win32DeviceWatcher::Start() {
  if (!Create(NULL, _T("libjingle Win32DeviceWatcher Window"),
              0, 0, 0, 0, 0, 0)) {
    return false;
  }

  audio_notify_ = Register(KSCATEGORY_AUDIO);
  if (!audio_notify_) {
    Stop();
    return false;
  }

  video_notify_ = Register(KSCATEGORY_VIDEO);
  if (!video_notify_) {
    Stop();
    return false;
  }

  return true;
}

void Win32DeviceWatcher::Stop() {
  UnregisterDeviceNotification(video_notify_);
  video_notify_ = NULL;
  UnregisterDeviceNotification(audio_notify_);
  audio_notify_ = NULL;
  Destroy();
}

HDEVNOTIFY Win32DeviceWatcher::Register(REFGUID guid) {
  DEV_BROADCAST_DEVICEINTERFACE dbdi;
  dbdi.dbcc_size = sizeof(dbdi);
  dbdi.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
  dbdi.dbcc_classguid = guid;
  dbdi.dbcc_name[0] = '\0';
  return RegisterDeviceNotification(handle(), &dbdi,
                                    DEVICE_NOTIFY_WINDOW_HANDLE);
}

void Win32DeviceWatcher::Unregister(HDEVNOTIFY handle) {
  UnregisterDeviceNotification(handle);
}

bool Win32DeviceWatcher::OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam,
                              LRESULT& result) {
  if (uMsg == WM_DEVICECHANGE) {
    if (wParam == DBT_DEVICEARRIVAL ||
        wParam == DBT_DEVICEREMOVECOMPLETE) {
      DEV_BROADCAST_DEVICEINTERFACE* dbdi =
          reinterpret_cast<DEV_BROADCAST_DEVICEINTERFACE*>(lParam);
      if (dbdi->dbcc_classguid == KSCATEGORY_AUDIO ||
        dbdi->dbcc_classguid == KSCATEGORY_VIDEO) {
        manager_->SignalDevicesChange();
      }
    }
    result = 0;
    return true;
  }

  return false;
}

};  // namespace cricket
