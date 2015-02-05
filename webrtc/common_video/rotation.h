/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_COMMON_VIDEO_ROTATION_H_
#define WEBRTC_COMMON_VIDEO_ROTATION_H_

#include "webrtc/base/common.h"

namespace webrtc {

// enum for clockwise rotation.
enum VideoFrameRotation {
  VideoFrameRotation_0 = 0,
  VideoFrameRotation_90 = 90,
  VideoFrameRotation_180 = 180,
  VideoFrameRotation_270 = 270
};

inline VideoFrameRotation ClockwiseRotationFromDegree(int rotation) {
  ASSERT(rotation == 0 || rotation == 90 || rotation == 180 || rotation == 270);
  return static_cast<webrtc::VideoFrameRotation>(rotation);
}

}  // namespace webrtc

#endif  // WEBRTC_COMMON_VIDEO_ROTATION_H_
