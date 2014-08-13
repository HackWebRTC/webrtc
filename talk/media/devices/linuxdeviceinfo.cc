/*
 * libjingle
 * Copyright 2012 Google Inc.
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

#include "talk/media/devices/deviceinfo.h"

#include "talk/media/devices/libudevsymboltable.h"
#include "webrtc/base/common.h"  // for ASSERT

namespace cricket {

class ScopedLibUdev {
 public:
  static ScopedLibUdev* Create() {
    ScopedLibUdev* ret_val = new ScopedLibUdev();
    if (!ret_val->Init()) {
      delete ret_val;
      return NULL;
    }
    return ret_val;
  }
  ~ScopedLibUdev() {
    libudev_.Unload();
  }

  LibUDevSymbolTable* instance() { return &libudev_; }

 private:
  ScopedLibUdev() {}

  bool Init() {
    return libudev_.Load() &&
           !IsWrongLibUDevAbiVersion(libudev_.GetDllHandle());
  }

  LibUDevSymbolTable libudev_;
};

class ScopedUdev {
 public:
  explicit ScopedUdev(LibUDevSymbolTable* libudev) : libudev_(libudev) {
    udev_ = libudev_->udev_new()();
  }
  ~ScopedUdev() {
    if (udev_) libudev_->udev_unref()(udev_);
  }

  udev* instance() { return udev_; }

 private:
  LibUDevSymbolTable* libudev_;
  udev* udev_;
};

class ScopedUdevEnumerate {
 public:
  ScopedUdevEnumerate(LibUDevSymbolTable* libudev, udev* udev)
      : libudev_(libudev) {
    enumerate_ = libudev_->udev_enumerate_new()(udev);
  }
  ~ScopedUdevEnumerate() {
    if (enumerate_) libudev_->udev_enumerate_unref()(enumerate_);
  }

  udev_enumerate* instance() { return enumerate_; }

 private:
  LibUDevSymbolTable* libudev_;
  udev_enumerate* enumerate_;
};

bool GetUsbProperty(const Device& device, const char* property_name,
                    std::string* property) {
  rtc::scoped_ptr<ScopedLibUdev> libudev_context(ScopedLibUdev::Create());
  if (!libudev_context) {
    return false;
  }
  ScopedUdev udev_context(libudev_context->instance());
  if (!udev_context.instance()) {
    return false;
  }
  ScopedUdevEnumerate enumerate_context(libudev_context->instance(),
                                        udev_context.instance());
  if (!enumerate_context.instance()) {
    return false;
  }
  libudev_context->instance()->udev_enumerate_add_match_subsystem()(
      enumerate_context.instance(), "video4linux");
  libudev_context->instance()->udev_enumerate_scan_devices()(
      enumerate_context.instance());
  udev_list_entry* devices =
      libudev_context->instance()->udev_enumerate_get_list_entry()(
          enumerate_context.instance());
  if (!devices) {
    return false;
  }
  udev_list_entry* dev_list_entry = NULL;
  const char* property_value = NULL;
  // Macro that expands to a for-loop over the devices.
  for (dev_list_entry = devices; dev_list_entry != NULL;
       dev_list_entry = libudev_context->instance()->
           udev_list_entry_get_next()(dev_list_entry)) {
    const char* path = libudev_context->instance()->udev_list_entry_get_name()(
        dev_list_entry);
    if (!path) continue;
    udev_device* dev =
        libudev_context->instance()->udev_device_new_from_syspath()(
            udev_context.instance(), path);
    if (!dev) continue;
    const char* device_node =
        libudev_context->instance()->udev_device_get_devnode()(dev);
    if (!device_node || device.id.compare(device_node) != 0) {
      continue;
    }
    dev = libudev_context->instance()->
        udev_device_get_parent_with_subsystem_devtype()(
            dev, "usb", "usb_device");
    if (!dev) continue;
    property_value = libudev_context->instance()->
        udev_device_get_sysattr_value()(
            dev, property_name);
    break;
  }
  if (!property_value) {
    return false;
  }
  property->assign(property_value);
  return true;
}

bool GetUsbId(const Device& device, std::string* usb_id) {
  std::string id_vendor;
  std::string id_product;
  if (!GetUsbProperty(device, "idVendor", &id_vendor)) {
    return false;
  }
  if (!GetUsbProperty(device, "idProduct", &id_product)) {
    return false;
  }
  usb_id->clear();
  usb_id->append(id_vendor);
  usb_id->append(":");
  usb_id->append(id_product);
  return true;
}

bool GetUsbVersion(const Device& device, std::string* usb_version) {
  return GetUsbProperty(device, "version", usb_version);
}

}  // namespace cricket
