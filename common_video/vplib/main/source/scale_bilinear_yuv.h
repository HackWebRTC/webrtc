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

#ifndef WEBRTC_COMMON_VIDEO_VPLIB_SCALE_BILINEAR_YUV_H
#define WEBRTC_COMMON_VIDEO_VPLIB_SCALE_BILINEAR_YUV_H

#include "typedefs.h"
#include "vplib.h"

namespace webrtc
{

WebRtc_Word32
ScaleBilinear(const WebRtc_UWord8* srcFrame, WebRtc_UWord8*& dstFrame,
              WebRtc_UWord32 srcWidth, WebRtc_UWord32 srcHeight,
              WebRtc_UWord32 dstWidth, WebRtc_UWord32 dstHeight,
              WebRtc_UWord32& dstSize);
}  // namespace webrtc

#endif  // WEBRTC_COMMON_VIDEO_VPLIB_SCALE_BILINEAR_YUV_H
