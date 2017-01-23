/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/mediatypes.h"
#include "webrtc/base/checks.h"

namespace cricket {

std::string MediaTypeToString(MediaType type) {
  std::string type_str;
  switch (type) {
    case MEDIA_TYPE_AUDIO:
      type_str = "audio";
      break;
    case MEDIA_TYPE_VIDEO:
      type_str = "video";
      break;
    case MEDIA_TYPE_DATA:
      type_str = "data";
      break;
    default:
      RTC_NOTREACHED();
      break;
  }
  return type_str;
}

}  // namespace cricket
