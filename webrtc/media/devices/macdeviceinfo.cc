/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/devices/deviceinfo.h"

namespace cricket {

bool GetUsbId(const Device& device, std::string* usb_id) {
  // Both PID and VID are 4 characters.
  const int id_size = 4;
  if (device.id.size() < 2 * id_size) {
    return false;
  }

  // The last characters of device id is a concatenation of VID and then PID.
  const size_t vid_location = device.id.size() - 2 * id_size;
  std::string id_vendor = device.id.substr(vid_location, id_size);
  const size_t pid_location = device.id.size() - id_size;
  std::string id_product = device.id.substr(pid_location, id_size);

  usb_id->clear();
  usb_id->append(id_vendor);
  usb_id->append(":");
  usb_id->append(id_product);
  return true;
}

bool GetUsbVersion(const Device& device, std::string* usb_version) {
  return false;
}

}  // namespace cricket
