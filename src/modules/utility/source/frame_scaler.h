/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file implements a class that can be used for scaling frames.
#ifndef WEBRTC_MODULES_UTILITY_SOURCE_FRAME_SCALER_H_
#define WEBRTC_MODULES_UTILITY_SOURCE_FRAME_SCALER_H_

#ifdef WEBRTC_MODULE_UTILITY_VIDEO

#include "engine_configurations.h"
#include "module_common_types.h"
#include "system_wrappers/interface/scoped_ptr.h"
#include "typedefs.h"

namespace webrtc
{
class Scaler;
class VideoFrame;
class FrameScaler
{
public:
    FrameScaler();
    ~FrameScaler();

    // Re-size videoFrame so that it has the width outWidth and height
    // outHeight.
    WebRtc_Word32 ResizeFrameIfNeeded(VideoFrame& videoFrame,
                                      WebRtc_UWord32 outWidth,
                                      WebRtc_UWord32 outHeight);
private:
    scoped_ptr<Scaler> _scaler;
    VideoFrame _scalerBuffer;
    WebRtc_UWord32 _outWidth;
    WebRtc_UWord32 _outHeight;
    WebRtc_UWord32 _inWidth;
    WebRtc_UWord32 _inHeight;

};
#endif // WEBRTC_MODULE_UTILITY_VIDEO
} // namespace webrtc
#endif // WEBRTC_MODULES_UTILITY_SOURCE_FRAME_SCALER_H_
