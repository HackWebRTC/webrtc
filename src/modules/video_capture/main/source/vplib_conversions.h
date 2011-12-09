/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_VPLIB_CONVERSIONS_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_VPLIB_CONVERSIONS_H_

#include "video_capture.h"
#include "common_video/libyuv/include/libyuv.h"

// TODO (mikhal): Update file name
namespace webrtc
{ 
namespace videocapturemodule
{
    VideoType RawVideoTypeToCommonVideoVideoType(RawVideoType type);
} // namespace videocapturemodule
} // namespace webrtc
#endif // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_VPLIB_CONVERSIONS_H_

