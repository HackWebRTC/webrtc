/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * scale_bilinear_yuv.h
 * yuv bilinear scaler
 */

#ifndef WEBRTC_COMMON_VIDEO_INTERFACE_SCALE_BILINEAR_YUV_H
#define WEBRTC_COMMON_VIDEO_INTERFACE_SCALE_BILINEAR_YUV_H

#include "typedefs.h"
#include "vplib.h"

namespace webrtc
{

WebRtc_Word32 ScaleBilinear(const WebRtc_UWord8* src, WebRtc_UWord8*& dst,
                            WebRtc_UWord32 sW, WebRtc_UWord32 sH,
                            WebRtc_UWord32 dW, WebRtc_UWord32 dH);

}  // namespace webrtc

#endif  // WEBRTC_COMMON_VIDEO_INTERFACE_SCALE_BILINEAR_YUV_H
