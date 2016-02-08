/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_DEVICES_DEVICEINFO_H_
#define WEBRTC_MEDIA_DEVICES_DEVICEINFO_H_

#include <string>

#include "webrtc/media/devices/devicemanager.h"

namespace cricket {

bool GetUsbId(const Device& device, std::string* usb_id);
bool GetUsbVersion(const Device& device, std::string* usb_version);

}  // namespace cricket

#endif  // WEBRTC_MEDIA_DEVICES_DEVICEINFO_H_
