/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>

#include "interpolator.h"
#include "scale_bilinear_yuv.h"

namespace webrtc
{

interpolator::interpolator():
_method(kBilinear),
_srcWidth(0),
_srcHeight(0),
_dstWidth(0),
_dstHeight(0)
{
}

interpolator:: ~interpolator()
{
    //
}

WebRtc_Word32
interpolator::Set(WebRtc_UWord32 srcWidth, WebRtc_UWord32 srcHeight,
                  WebRtc_UWord32 dstWidth, WebRtc_UWord32 dstHeight,
                  VideoType srcVideoType, VideoType dstVideoType,
                  interpolatorType type)
{
    if (srcWidth < 1 || srcHeight < 1 || dstWidth < 1 || dstHeight < 1 )
        return -1;

    if (Method(type) < 0)
        return -1;

    if (!SupportedVideoType(srcVideoType, dstVideoType))
        return -1;

    _srcWidth = srcWidth;
    _srcHeight = srcHeight;
    _dstWidth = dstWidth;
    _dstHeight = dstHeight;
    return 0;
}


WebRtc_Word32
interpolator::Interpolate(const WebRtc_UWord8* srcFrame,
                          WebRtc_UWord8*& dstFrame)
{
    if (srcFrame == NULL)
        return -1;

    switch (_method)
    {
        case kBilinear :
            return ScaleBilinear (srcFrame, dstFrame,
                                  _srcWidth, _srcHeight,
                                  _dstWidth, _dstHeight);
        default :
            return -1;
    }
}



WebRtc_Word32
interpolator::Method(interpolatorType type)
{
    _method = type;

    return 0;
}


WebRtc_Word32
interpolator::SupportedVideoType(VideoType srcVideoType,
                                 VideoType dstVideoType)
{
    if (srcVideoType != dstVideoType)
        return -1;

    if ((srcVideoType != kI420) ||
        (srcVideoType != kIYUV) ||
        (srcVideoType != kYV12))
        return -1;

    return 0;
}

}  // namespace webrtc
