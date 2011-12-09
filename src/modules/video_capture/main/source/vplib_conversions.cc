/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vplib_conversions.h"

namespace webrtc
{
namespace videocapturemodule
{
VideoType RawVideoTypeToCommonVideoVideoType(RawVideoType type)
{
    switch (type)
    {
        case kVideoI420:
            return kI420;
        case kVideoIYUV:
            return kIYUV;
        case kVideoRGB24:
            return kRGB24;
        case kVideoARGB:
            return kARGB;
        case kVideoARGB4444:
            return kARGB4444;
        case kVideoRGB565:
            return kRGB565;
        case kVideoARGB1555:
            return kARGB1555;
        case kVideoYUY2:
            return kYUY2;
        case kVideoYV12:
            return kYV12;
        case kVideoUYVY:
            return kUYVY;
        case kVideoNV21:
            return kNV21;
        case kVideoNV12:
            return kNV12;
        default:
            assert(!"RawVideoTypeToCommonVideoVideoType unknown type");
    }
    return kUnknown;
}
} //namespace videocapturemodule
}//namespace webrtc
