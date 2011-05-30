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
 * interpolator.h
 * Interface to the WebRTC's interpolation functionality
 */

#ifndef WEBRTC_COMMON_VIDEO_INTERFACE_INTERPOLATOR_H
#define WEBRTC_COMMON_VIDEO_INTERFACE_INTERPOLATOR_H

#include "typedefs.h"
#include "vplib.h"

namespace webrtc
{

// supported interpolation types
enum interpolatorType
{
    kBilinear
};


class interpolator
{
public:
    interpolator();
    ~interpolator();

    // Set interpolation properties:
    //
    // Return value     : 0 if OK,
    //                  : -1 - parameter error
    //                  : -2 - general error
    WebRtc_Word32 Set(WebRtc_UWord32 srcWidth, WebRtc_UWord32 srcHeight,
                      WebRtc_UWord32 dstWidth, WebRtc_UWord32 dstHeight,
                      VideoType srcVideoType, VideoType dstVideoType,
                      interpolatorType type);

    // Interpolate frame
    //
    // Return value     : Height of interpolated frame if OK,
    //                  : -1 - parameter error
    //                  : -2 - general error
    WebRtc_Word32 Interpolate(const WebRtc_UWord8* srcFrame,
                              WebRtc_UWord8*& dstFrame);

private:

    // Extract computation method given actual type
    WebRtc_Word32 Method(interpolatorType type);

    // Determine if the VideoTypes are currently supported
    WebRtc_Word32 SupportedVideoType(VideoType srcVideoType,
                                     VideoType dstVideoType);

    interpolatorType        _method;
    WebRtc_UWord32          _srcWidth;
    WebRtc_UWord32          _srcHeight;
    WebRtc_UWord32          _dstWidth;
    WebRtc_UWord32          _dstHeight;
};


}  // namespace webrtc


#endif  // WEBRTC_COMMON_VIDEO_INTERFACE_INTERPOLATOR_H
