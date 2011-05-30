/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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

#include "talk/session/phone/devicemanager.h"

#if WIN32
#include <atlbase.h>
#include <dbt.h>
#include <strmif.h>  // must come before ks.h
#include <mmsystem.h>
#include <ks.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <uuids.h>
#include "talk/base/win32.h"  // ToUtf8
#include "talk/base/win32window.h"

// PKEY_AudioEndpoint_GUID isn't included in uuid.lib and we don't want
// to define INITGUID in order to define all the uuids in this object file
// as it will conflict with uuid.lib (multiply defined symbols).
// So our workaround is to define this one missing symbol here manually.
EXTERN_C const PROPERTYKEY PKEY_AudioEndpoint_GUID = { {
  0x1da5d803, 0xd492, 0x4edd, {
    0x8c, 0x23, 0xe0, 0xc0, 0xff, 0xee, 0x7f, 0x0e
  } }, 4
};

#elif OSX
#include <CoreAudio/CoreAudio.h>
#include <QuickTime/QuickTime.h>
#elif LINUX
#include <libudev.h>
#include <unistd.h>
#include "talk/base/linux.h"
#include "talk/base/fileutils.h"
#include "talk/base/pathutils.h"
#include "talk/base/physicalsocketserver.h"
#include "talk/base/stream.h"
#include "talk/session/phone/libudevsymboltable.h"
#include "talk/session/phone/v4llookup.h"
#if defined(LINUX_SOUND_USED)
#include "talk/sound/platformsoundsystem.h"
#include "talk/sound/platformsoundsystemfactory.h"
#include "talk/sound/sounddevicelocator.h"
#include "talk/sound/soundsysteminterface.h"
#endif
#endif

#include "talk/base/logging.h"
#include "talk/base/stringutils.h"
#include "talk/session/phone/mediaengine.h"

namespace cricket {
// Initialize to empty string.
const std::string DeviceManager::kDefaultDeviceName;

#ifdef PLATFORM_CHROMIUM
class DeviceWatcher {
 public:
  explicit DeviceWatcher(DeviceManager* dm);
  bool Start();
  void Stop();
};
#elif defined(WIN32)
class DeviceWatcher : public talk_base::Win32Window {
 public:
  explicit DeviceWatcher(DeviceManager* dm);
  bool Start();
  void Stop();

 private:
  HDEVNOTIFY Register(REFGUID guid);
  void Unregister(HDEVNOTIFY notify);
  virtual bool OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT& result);

  DeviceManager* manager_;
  HDEVNOTIFY audio_notify_;
  HDEVNOTIFY video_notify_;
};
#elif defined(LINUX)
class DeviceWatcher : private talk_base::Dispatcher {
 public:
  explicit DeviceWatcher(DeviceManager* dm);
  bool Start();
  void Stop();

 private:
  virtual uint32 GetRequestedEvents();
  virtual void OnPreEvent(uint32 ff);
  virtual void OnEvent(uint32 ff, int err);
  virtual int GetDescriptor();
  virtual bool IsDescriptorClosed();

  DeviceManager* manager_;
  LibUDevSymbolTable libudev_;
  struct udev* udev_;
  struct udev_monitor* udev_monitor_;
  bool registered_;
};
#define LATE(sym) LATESYM_GET(LibUDevSymbolTable, &libudev_, sym)
#elif defined(OSX)
class DeviceWatcher {
 public:
  explicit DeviceWatcher(DeviceManager* dm);
  bool Start();
  void Stop();
 private:
  DeviceManager* manager_;
  void* impl_;
};
#endif

#if !defined(LINUX) && !defined(IOS)
static bool ShouldDeviceBeIgnored(const std::string& device_name);
#endif
#ifndef OSX
static bool GetVideoDevices(std::vector<Device>* out);
#endif
#if WIN32
static const wchar_t kFriendlyName[] = L"FriendlyName";
static const wchar_t kDevicePath[] = L"DevicePath";
static const char kUsbDevicePathPrefix[] = "\\\\?\\usb";
static bool GetDevices(const CLSID& catid, std::vector<Device>* out);
static bool GetCoreAudioDevices(bool input, std::vector<Device>* devs);
static bool GetWaveDevices(bool input, std::vector<Device>* devs);
#elif OSX
static const int kVideoDeviceOpenAttempts = 3;
static const UInt32 kAudioDeviceNameLength = 64;
// Obj-C functions defined in devicemanager-mac.mm
extern void* CreateDeviceWatcherCallback(DeviceManager* dm);
extern void ReleaseDeviceWatcherCallback(void* impl);
extern bool GetQTKitVideoDevices(std::vector<Device>* out);
static bool GetAudioDeviceIDs(bool inputs, std::vector<AudioDeviceID>* out);
static bool GetAudioDeviceName(AudioDeviceID id, bool input, std::string* out);
#endif

DeviceManager::DeviceManager()
    : initialized_(false),
#if defined(WIN32)
      need_couninitialize_(false),
#endif
      watcher_(new DeviceWatcher(this))
#ifdef LINUX_SOUND_USED
      , sound_system_(new PlatformSoundSystemFactory())
#endif
    {
}

DeviceManager::~DeviceManager() {
  if (initialized_) {
    Terminate();
  }
  delete watcher_;
}

bool DeviceManager::Init() {
  if (!initialized_) {
#if defined(WIN32) && !defined(PLATFORM_CHROMIUM)
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    need_couninitialize_ = SUCCEEDED(hr);
    if (FAILED(hr)) {
      LOG(LS_ERROR) << "CoInitialize failed, hr=" << hr;
      if (hr != RPC_E_CHANGED_MODE) {
        return false;
      }
    }
#endif
    if (!watcher_->Start()) {
      return false;
    }
    initialized_ = true;
  }
  return true;
}

void DeviceManager::Terminate() {
  if (initialized_) {
    watcher_->Stop();
#if defined(WIN32) && !defined(PLATFORM_CHROMIUM)
    if (need_couninitialize_) {
      CoUninitialize();
      need_couninitialize_ = false;
    }
#endif
    initialized_ = false;
  }
}

int DeviceManager::GetCapabilities() {
  std::vector<Device> devices;
  int caps = MediaEngine::VIDEO_RECV;
  if (GetAudioInputDevices(&devices) && !devices.empty()) {
    caps |= MediaEngine::AUDIO_SEND;
  }
  if (GetAudioOutputDevices(&devices) && !devices.empty()) {
    caps |= MediaEngine::AUDIO_RECV;
  }
  if (GetVideoCaptureDevices(&devices) && !devices.empty()) {
    caps |= MediaEngine::VIDEO_SEND;
  }
  return caps;
}

bool DeviceManager::GetAudioInputDevices(std::vector<Device>* devices) {
  return GetAudioDevicesByPlatform(true, devices);
}

bool DeviceManager::GetAudioOutputDevices(std::vector<Device>* devices) {
  return GetAudioDevicesByPlatform(false, devices);
}

bool DeviceManager::GetAudioInputDevice(const std::string& name, Device* out) {
  return GetAudioDevice(true, name, out);
}

bool DeviceManager::GetAudioOutputDevice(const std::string& name, Device* out) {
  return GetAudioDevice(false, name, out);
}

#ifdef OSX
static bool FilterDevice(const Device& d) {
  return ShouldDeviceBeIgnored(d.name);
}
#endif

bool DeviceManager::GetVideoCaptureDevices(std::vector<Device>* devices) {
  devices->clear();
#ifdef PLATFORM_CHROMIUM
  devices->push_back(Device("", -1));
  return true;
#elif OSX
  if (GetQTKitVideoDevices(devices)) {
    // Now filter out any known incompatible devices
    devices->erase(remove_if(devices->begin(), devices->end(), FilterDevice),
                   devices->end());
    return true;
  }
  return false;
#else
  return GetVideoDevices(devices);
#endif
}

bool DeviceManager::GetDefaultVideoCaptureDevice(Device* device) {
  bool ret = false;
#ifdef PLATFORM_CHROMIUM
  *device = Device("", -1);
  ret = true;
#elif WIN32
  // If there are multiple capture devices, we want the first USB one.
  // This avoids issues with defaulting to virtual cameras or grabber cards.
  std::vector<Device> devices;
  ret = (GetVideoDevices(&devices) && !devices.empty());
  if (ret) {
    *device = devices[0];
    for (size_t i = 0; i < devices.size(); ++i) {
      if (strnicmp(devices[i].id.c_str(), kUsbDevicePathPrefix,
                   ARRAY_SIZE(kUsbDevicePathPrefix) - 1) == 0) {
        *device = devices[i];
        break;
      }
    }
  }
#else
  // We just return the first device.
  std::vector<Device> devices;
  ret = (GetVideoCaptureDevices(&devices) && !devices.empty());
  if (ret) {
    *device = devices[0];
  }
#endif
  return ret;
}

bool DeviceManager::GetVideoCaptureDevice(const std::string& name,
                                          Device* out) {
  // If the name is empty, return the default device.
  if (name.empty() || name == kDefaultDeviceName) {
    return GetDefaultVideoCaptureDevice(out);
  }

  std::vector<Device> devices;
  if (!GetVideoCaptureDevices(&devices)) {
    return false;
  }

#ifdef PLATFORM_CHROMIUM
  *out = Device(name, name);
  return true;
#else
  for (std::vector<Device>::const_iterator it = devices.begin();
      it != devices.end(); ++it) {
    if (name == it->name) {
      *out = *it;
      return true;
    }
  }
#endif

  return false;
}

bool DeviceManager::GetAudioDevice(bool is_input, const std::string& name,
                                   Device* out) {
  // If the name is empty, return the default device id.
  if (name.empty() || name == kDefaultDeviceName) {
    *out = Device(name, -1);
    return true;
  }

  std::vector<Device> devices;
  bool ret = is_input ? GetAudioInputDevices(&devices) :
                        GetAudioOutputDevices(&devices);
  if (ret) {
    ret = false;
    for (size_t i = 0; i < devices.size(); ++i) {
      if (devices[i].name == name) {
        *out = devices[i];
        ret = true;
        break;
      }
    }
  }
  return ret;
}

bool DeviceManager::GetAudioDevicesByPlatform(bool input,
                                              std::vector<Device>* devs) {
  devs->clear();
#ifdef PLATFORM_CHROMIUM
  devs->push_back(Device("", -1));
  return true;
#elif defined(LINUX_SOUND_USED)
  if (!sound_system_.get()) {
    return false;
  }
  SoundSystemInterface::SoundDeviceLocatorList list;
  bool success;
  if (input) {
    success = sound_system_->EnumerateCaptureDevices(&list);
  } else {
    success = sound_system_->EnumeratePlaybackDevices(&list);
  }
  if (!success) {
    LOG(LS_ERROR) << "Can't enumerate devices";
    sound_system_.release();
    return false;
  }
  // We have to start the index at 1 because GIPS VoiceEngine puts the default
  // device at index 0, but Enumerate(Capture|Playback)Devices does not include
  // a locator for the default device.
  int index = 1;
  for (SoundSystemInterface::SoundDeviceLocatorList::iterator i = list.begin();
       i != list.end();
       ++i, ++index) {
    devs->push_back(Device((*i)->name(), index));
  }
  SoundSystemInterface::ClearSoundDeviceLocatorList(&list);
  sound_system_.release();
  return true;

#elif defined(WIN32)
  if (talk_base::IsWindowsVistaOrLater()) {
    return GetCoreAudioDevices(input, devs);
  } else {
    return GetWaveDevices(input, devs);
  }

#elif defined(OSX)
  std::vector<AudioDeviceID> dev_ids;
  bool ret = GetAudioDeviceIDs(input, &dev_ids);
  if (ret) {
    for (size_t i = 0; i < dev_ids.size(); ++i) {
      std::string name;
      if (GetAudioDeviceName(dev_ids[i], input, &name)) {
        devs->push_back(Device(name, dev_ids[i]));
      }
    }
  }
  return ret;

#else
  return false;
#endif
}

#if defined(PLATFORM_CHROMIUM)
DeviceWatcher::DeviceWatcher(DeviceManager* manager) {
}

bool DeviceWatcher::Start() {
  return true;
}

void DeviceWatcher::Stop() {
}

#elif defined(WIN32)
bool GetVideoDevices(std::vector<Device>* devices) {
  return GetDevices(CLSID_VideoInputDeviceCategory, devices);
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
      CComPtr<IPropertyBag> bag;
      if (SUCCEEDED(mk->BindToStorage(NULL, NULL,
          __uuidof(bag), reinterpret_cast<void**>(&bag)))) {
        CComVariant name, path;
        std::string name_str, path_str;
        if (SUCCEEDED(bag->Read(kFriendlyName, &name, 0)) &&
            name.vt == VT_BSTR) {
          name_str = talk_base::ToUtf8(name.bstrVal);
          if (!ShouldDeviceBeIgnored(name_str)) {
            // Get the device id if one exists.
            if (SUCCEEDED(bag->Read(kDevicePath, &path, 0)) &&
                path.vt == VT_BSTR) {
              path_str = talk_base::ToUtf8(path.bstrVal);
            }

            devices->push_back(Device(name_str, path_str));
          }
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
      *out = talk_base::ToUtf8(var.pwszVal);
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

bool GetCoreAudioDevices(bool input, std::vector<Device>* devs) {
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

  if (!SUCCEEDED(hr)) {
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
        devs->push_back(Device(talk_base::ToUtf8(caps.szPname),
                               talk_base::ToString(i)));
      }
    }
  } else {
    int num_devs = waveOutGetNumDevs();
    for (int i = 0; i < num_devs; ++i) {
      WAVEOUTCAPS caps;
      if (waveOutGetDevCaps(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR &&
          caps.wChannels > 0) {
        devs->push_back(Device(talk_base::ToUtf8(caps.szPname), i));
      }
    }
  }
  return true;
}

DeviceWatcher::DeviceWatcher(DeviceManager* manager)
    : manager_(manager), audio_notify_(NULL), video_notify_(NULL) {
}

bool DeviceWatcher::Start() {
  if (!Create(NULL, _T("libjingle DeviceWatcher Window"),
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

void DeviceWatcher::Stop() {
  UnregisterDeviceNotification(video_notify_);
  video_notify_ = NULL;
  UnregisterDeviceNotification(audio_notify_);
  audio_notify_ = NULL;
  Destroy();
}

HDEVNOTIFY DeviceWatcher::Register(REFGUID guid) {
  DEV_BROADCAST_DEVICEINTERFACE dbdi;
  dbdi.dbcc_size = sizeof(dbdi);
  dbdi.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
  dbdi.dbcc_classguid = guid;
  dbdi.dbcc_name[0] = '\0';
  return RegisterDeviceNotification(handle(), &dbdi,
                                    DEVICE_NOTIFY_WINDOW_HANDLE);
}

void DeviceWatcher::Unregister(HDEVNOTIFY handle) {
  UnregisterDeviceNotification(handle);
}

bool DeviceWatcher::OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam,
                              LRESULT& result) {
  if (uMsg == WM_DEVICECHANGE) {
    if (wParam == DBT_DEVICEARRIVAL ||
        wParam == DBT_DEVICEREMOVECOMPLETE) {
      DEV_BROADCAST_DEVICEINTERFACE* dbdi =
          reinterpret_cast<DEV_BROADCAST_DEVICEINTERFACE*>(lParam);
      if (dbdi->dbcc_classguid == KSCATEGORY_AUDIO ||
        dbdi->dbcc_classguid == KSCATEGORY_VIDEO) {
        manager_->OnDevicesChange();
      }
    }
    result = 0;
    return true;
  }

  return false;
}
#elif defined(OSX)
static bool GetAudioDeviceIDs(bool input,
                              std::vector<AudioDeviceID>* out_dev_ids) {
  UInt32 propsize;
  OSErr err = AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices,
                                           &propsize, NULL);
  if (0 != err) {
    LOG(LS_ERROR) << "Couldn't get information about property, "
                  << "so no device list acquired.";
    return false;
  }

  size_t num_devices = propsize / sizeof(AudioDeviceID);
  talk_base::scoped_array<AudioDeviceID> device_ids(
      new AudioDeviceID[num_devices]);

  err = AudioHardwareGetProperty(kAudioHardwarePropertyDevices,
                                 &propsize, device_ids.get());
  if (0 != err) {
    LOG(LS_ERROR) << "Failed to get device ids, "
                  << "so no device listing acquired.";
    return false;
  }

  for (size_t i = 0; i < num_devices; ++i) {
    AudioDeviceID an_id = device_ids[i];
    // find out the number of channels for this direction
    // (input/output) on this device -
    // we'll ignore anything with no channels.
    err = AudioDeviceGetPropertyInfo(an_id, 0, input,
                                     kAudioDevicePropertyStreams,
                                     &propsize, NULL);
    if (0 == err) {
      unsigned num_channels = propsize / sizeof(AudioStreamID);
      if (0 < num_channels) {
        out_dev_ids->push_back(an_id);
      }
    } else {
      LOG(LS_ERROR) << "No property info for stream property for device id "
                    << an_id << "(is_input == " << input
                    << "), so not including it in the list.";
    }
  }

  return true;
}

static bool GetAudioDeviceName(AudioDeviceID id,
                               bool input,
                               std::string* out_name) {
  UInt32 nameLength = kAudioDeviceNameLength;
  char name[kAudioDeviceNameLength + 1];
  OSErr err = AudioDeviceGetProperty(id, 0, input,
                                     kAudioDevicePropertyDeviceName,
                                     &nameLength, name);
  if (0 != err) {
    LOG(LS_ERROR) << "No name acquired for device id " << id;
    return false;
  }

  *out_name = name;
  return true;
}

DeviceWatcher::DeviceWatcher(DeviceManager* manager)
    : manager_(manager), impl_(NULL) {
}

bool DeviceWatcher::Start() {
  if (!impl_) {
    impl_ = CreateDeviceWatcherCallback(manager_);
  }
  return impl_ != NULL;
}

void DeviceWatcher::Stop() {
  if (impl_) {
    ReleaseDeviceWatcherCallback(impl_);
    impl_ = NULL;
  }
}

#elif defined(LINUX)
static const std::string kVideoMetaPathK2_4("/proc/video/dev/");
static const std::string kVideoMetaPathK2_6("/sys/class/video4linux/");

enum MetaType { M2_4, M2_6, NONE };

static void ScanDeviceDirectory(const std::string& devdir,
                                std::vector<Device>* devices) {
  talk_base::scoped_ptr<talk_base::DirectoryIterator> directoryIterator(
      talk_base::Filesystem::IterateDirectory());

  if (directoryIterator->Iterate(talk_base::Pathname(devdir))) {
    do {
      std::string filename = directoryIterator->Name();
      std::string device_name = devdir + filename;
      if (!directoryIterator->IsDots()) {
        if (filename.find("video") == 0 &&
            V4LLookup::IsV4L2Device(device_name)) {
          devices->push_back(Device(device_name, device_name));
        }
      }
    } while (directoryIterator->Next());
  }
}

static std::string GetVideoDeviceNameK2_6(const std::string& device_meta_path) {
  std::string device_name;

  talk_base::scoped_ptr<talk_base::FileStream> device_meta_stream(
      talk_base::Filesystem::OpenFile(device_meta_path, "r"));

  if (device_meta_stream.get() != NULL) {
    if (device_meta_stream->ReadLine(&device_name) != talk_base::SR_SUCCESS) {
      LOG(LS_ERROR) << "Failed to read V4L2 device meta " << device_meta_path;
    }
    device_meta_stream->Close();
  }

  return device_name;
}

static std::string Trim(const std::string& s, const std::string& drop = " \t") {
  std::string::size_type first = s.find_first_not_of(drop);
  std::string::size_type last  = s.find_last_not_of(drop);

  if (first == std::string::npos || last == std::string::npos)
    return std::string("");

  return s.substr(first, last - first + 1);
}

static std::string GetVideoDeviceNameK2_4(const std::string& device_meta_path) {
  talk_base::ConfigParser::MapVector all_values;

  talk_base::ConfigParser config_parser;
  talk_base::FileStream* file_stream =
      talk_base::Filesystem::OpenFile(device_meta_path, "r");

  if (file_stream == NULL) return "";

  config_parser.Attach(file_stream);
  config_parser.Parse(&all_values);

  for (talk_base::ConfigParser::MapVector::iterator i = all_values.begin();
      i != all_values.end(); ++i) {
    talk_base::ConfigParser::SimpleMap::iterator device_name_i =
        i->find("name");

    if (device_name_i != i->end()) {
      return device_name_i->second;
    }
  }

  return "";
}

static std::string GetVideoDeviceName(MetaType meta,
    const std::string& device_file_name) {
  std::string device_meta_path;
  std::string device_name;
  std::string meta_file_path;

  if (meta == M2_6) {
    meta_file_path = kVideoMetaPathK2_6 + device_file_name + "/name";

    LOG(LS_INFO) << "Trying " + meta_file_path;
    device_name = GetVideoDeviceNameK2_6(meta_file_path);

    if (device_name.empty()) {
      meta_file_path = kVideoMetaPathK2_6 + device_file_name + "/model";

      LOG(LS_INFO) << "Trying " << meta_file_path;
      device_name = GetVideoDeviceNameK2_6(meta_file_path);
    }
  } else {
    meta_file_path = kVideoMetaPathK2_4 + device_file_name;
    LOG(LS_INFO) << "Trying " << meta_file_path;
    device_name = GetVideoDeviceNameK2_4(meta_file_path);
  }

  if (device_name.empty()) {
    device_name = "/dev/" + device_file_name;
    LOG(LS_ERROR)
      << "Device name not found, defaulting to device path " << device_name;
  }

  LOG(LS_INFO) << "Name for " << device_file_name << " is " << device_name;

  return Trim(device_name);
}

static void ScanV4L2Devices(std::vector<Device>* devices) {
  LOG(LS_INFO) << ("Enumerating V4L2 devices");

  MetaType meta;
  std::string metadata_dir;

  talk_base::scoped_ptr<talk_base::DirectoryIterator> directoryIterator(
      talk_base::Filesystem::IterateDirectory());

  // Try and guess kernel version
  if (directoryIterator->Iterate(kVideoMetaPathK2_6)) {
    meta = M2_6;
    metadata_dir = kVideoMetaPathK2_6;
  } else if (directoryIterator->Iterate(kVideoMetaPathK2_4)) {
    meta = M2_4;
    metadata_dir = kVideoMetaPathK2_4;
  } else {
    meta = NONE;
  }

  if (meta != NONE) {
    LOG(LS_INFO) << "V4L2 device metadata found at " << metadata_dir;

    do {
      std::string filename = directoryIterator->Name();

      if (filename.find("video") == 0) {
        std::string device_path = "/dev/" + filename;

        if (V4LLookup::IsV4L2Device(device_path)) {
          devices->push_back(
              Device(GetVideoDeviceName(meta, filename), device_path));
        }
      }
    } while (directoryIterator->Next());
  } else {
    LOG(LS_ERROR) << "Unable to detect v4l2 metadata directory";
  }

  if (devices->size() == 0) {
    LOG(LS_INFO) << "Plan B. Scanning all video devices in /dev directory";
    ScanDeviceDirectory("/dev/", devices);
  }

  LOG(LS_INFO) << "Total V4L2 devices found : " << devices->size();
}

static bool GetVideoDevices(std::vector<Device>* devices) {
  ScanV4L2Devices(devices);
  return true;
}

DeviceWatcher::DeviceWatcher(DeviceManager* dm)
    : manager_(dm), udev_(NULL), udev_monitor_(NULL), registered_(false) {}

bool DeviceWatcher::Start() {
  // We deliberately return true in the failure paths here because libudev is
  // not a critical component of a Linux system so it may not be present/usable,
  // and we don't want to halt DeviceManager initialization in such a case.
  if (!libudev_.Load()) {
    LOG(LS_WARNING) << "libudev not present/usable; DeviceWatcher disabled";
    return true;
  }
  udev_ = LATE(udev_new)();
  if (!udev_) {
    LOG_ERR(LS_ERROR) << "udev_new()";
    return true;
  }
  // The second argument here is the event source. It can be either "kernel" or
  // "udev", but "udev" is the only correct choice. Apps listen on udev and the
  // udev daemon in turn listens on the kernel.
  udev_monitor_ = LATE(udev_monitor_new_from_netlink)(udev_, "udev");
  if (!udev_monitor_) {
    LOG_ERR(LS_ERROR) << "udev_monitor_new_from_netlink()";
    return true;
  }
  // We only listen for changes in the video devices. Audio devices are more or
  // less unimportant because receiving device change notifications really only
  // matters for broadcasting updated send/recv capabilities based on whether
  // there is at least one device available, and almost all computers have at
  // least one audio device. Also, PulseAudio device notifications don't come
  // from the udev daemon, they come from the PulseAudio daemon, so we'd only
  // want to listen for audio device changes from udev if using ALSA. For
  // simplicity, we don't bother with any audio stuff at all.
  if (LATE(udev_monitor_filter_add_match_subsystem_devtype)(udev_monitor_,
                                                            "video4linux",
                                                            NULL) < 0) {
    LOG_ERR(LS_ERROR) << "udev_monitor_filter_add_match_subsystem_devtype()";
    return true;
  }
  if (LATE(udev_monitor_enable_receiving)(udev_monitor_) < 0) {
    LOG_ERR(LS_ERROR) << "udev_monitor_enable_receiving()";
    return true;
  }
  static_cast<talk_base::PhysicalSocketServer*>(
      talk_base::Thread::Current()->socketserver())->Add(this);
  registered_ = true;
  return true;
}

void DeviceWatcher::Stop() {
  if (registered_) {
    static_cast<talk_base::PhysicalSocketServer*>(
        talk_base::Thread::Current()->socketserver())->Remove(this);
    registered_ = false;
  }
  if (udev_monitor_) {
    LATE(udev_monitor_unref)(udev_monitor_);
    udev_monitor_ = NULL;
  }
  if (udev_) {
    LATE(udev_unref)(udev_);
    udev_ = NULL;
  }
  libudev_.Unload();
}

uint32 DeviceWatcher::GetRequestedEvents() {
  return talk_base::DE_READ;
}

void DeviceWatcher::OnPreEvent(uint32 ff) {
  // Nothing to do.
}

void DeviceWatcher::OnEvent(uint32 ff, int err) {
  udev_device* device = LATE(udev_monitor_receive_device)(udev_monitor_);
  if (!device) {
    // Probably the socket connection to the udev daemon was terminated (perhaps
    // the daemon crashed or is being restarted?).
    LOG_ERR(LS_WARNING) << "udev_monitor_receive_device()";
    // Stop listening to avoid potential livelock (an fd with EOF in it is
    // always considered readable).
    static_cast<talk_base::PhysicalSocketServer*>(
        talk_base::Thread::Current()->socketserver())->Remove(this);
    registered_ = false;
    return;
  }
  // Else we read the device successfully.

  // Since we already have our own filesystem-based device enumeration code, we
  // simply re-enumerate rather than inspecting the device event.
  LATE(udev_device_unref)(device);
  manager_->OnDevicesChange();
}

int DeviceWatcher::GetDescriptor() {
  return LATE(udev_monitor_get_fd)(udev_monitor_);
}

bool DeviceWatcher::IsDescriptorClosed() {
  // If it is closed then we will just get an error in
  // udev_monitor_receive_device and unregister, so we don't need to check for
  // it separately.
  return false;
}

#endif

// TODO: Try to get hold of a copy of Final Cut to understand why we
//               crash while scanning their components on OS X.
#if !defined(LINUX) && !defined(IOS)
static bool ShouldDeviceBeIgnored(const std::string& device_name) {
  static const char* const kFilteredDevices[] =  {
      "Google Camera Adapter",   // Our own magiccams
#ifdef WIN32
      "Asus virtual Camera",     // Bad Asus desktop virtual cam
      "Bluetooth Video",         // Bad Sony viao bluetooth sharing driver
#elif OSX
      "DVCPRO HD",               // Final cut
      "Sonix SN9C201p",          // Crashes in OpenAComponent and CloseComponent
#endif
  };

  for (int i = 0; i < ARRAY_SIZE(kFilteredDevices); ++i) {
    if (strnicmp(device_name.c_str(), kFilteredDevices[i],
        strlen(kFilteredDevices[i])) == 0) {
      LOG(LS_INFO) << "Ignoring device " << device_name;
      return true;
    }
  }
  return false;
}
#endif

};  // namespace cricket
