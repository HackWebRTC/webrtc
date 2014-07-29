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

#ifndef TALK_MEDIA_DEVICES_LIBUDEVSYMBOLTABLE_H_
#define TALK_MEDIA_DEVICES_LIBUDEVSYMBOLTABLE_H_

#include <libudev.h>

#include "webrtc/base/latebindingsymboltable.h"

namespace cricket {

#define LIBUDEV_SYMBOLS_CLASS_NAME LibUDevSymbolTable
// The libudev symbols we need, as an X-Macro list.
// This list must contain precisely every libudev function that is used in
// linuxdevicemanager.cc.
#define LIBUDEV_SYMBOLS_LIST \
  X(udev_device_get_devnode) \
  X(udev_device_get_parent_with_subsystem_devtype) \
  X(udev_device_get_sysattr_value) \
  X(udev_device_new_from_syspath) \
  X(udev_device_unref) \
  X(udev_enumerate_add_match_subsystem) \
  X(udev_enumerate_get_list_entry) \
  X(udev_enumerate_new) \
  X(udev_enumerate_scan_devices) \
  X(udev_enumerate_unref) \
  X(udev_list_entry_get_name) \
  X(udev_list_entry_get_next) \
  X(udev_monitor_enable_receiving) \
  X(udev_monitor_filter_add_match_subsystem_devtype) \
  X(udev_monitor_get_fd) \
  X(udev_monitor_new_from_netlink) \
  X(udev_monitor_receive_device) \
  X(udev_monitor_unref) \
  X(udev_new) \
  X(udev_unref)

#define LATE_BINDING_SYMBOL_TABLE_CLASS_NAME LIBUDEV_SYMBOLS_CLASS_NAME
#define LATE_BINDING_SYMBOL_TABLE_SYMBOLS_LIST LIBUDEV_SYMBOLS_LIST
#include "webrtc/base/latebindingsymboltable.h.def"
#undef LATE_BINDING_SYMBOL_TABLE_CLASS_NAME
#undef LATE_BINDING_SYMBOL_TABLE_SYMBOLS_LIST

// libudev has changed ABIs to libudev.so.1 in recent distros and lots of users
// and/or software (including Google Chrome) are symlinking the old to the new.
// The entire point of ABI versions is that you can't safely do that, and
// it has caused crashes in the wild. This function checks if the DllHandle that
// we got back for libudev.so.0 is actually for libudev.so.1. If so, the library
// cannot safely be used.
bool IsWrongLibUDevAbiVersion(rtc::DllHandle libudev_0);

}  // namespace cricket

#endif  // TALK_MEDIA_DEVICES_LIBUDEVSYMBOLTABLE_H_
