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
 * spatial_resampler.h
 */

#ifndef VPM_SPATIAL_RESAMPLER_H
#define VPM_SPATIAL_RESAMPLER_H

#include "typedefs.h"

#include "module_common_types.h"
#include "video_processing_defines.h"

#include "vplib.h"
#include "interpolator.h"

namespace webrtc {

class VPMSpatialResampler
{
public:
    virtual ~VPMSpatialResampler() {};
    virtual WebRtc_Word32 SetTargetFrameSize(WebRtc_UWord32 width,
                                             WebRtc_UWord32 height) = 0;
    virtual void SetInputFrameResampleMode(VideoFrameResampling
                                           resamplingMode) = 0;
    virtual void Reset() = 0;
    virtual WebRtc_Word32 ResampleFrame(const VideoFrame& inFrame,
                                        VideoFrame& outFrame) = 0;
    virtual WebRtc_UWord32 TargetWidth() = 0;
    virtual WebRtc_UWord32 TargetHeight() = 0;
    virtual WebRtc_Word32 Release() = 0;
};

class VPMSimpleSpatialResampler : public VPMSpatialResampler
{
public:
    VPMSimpleSpatialResampler();
    ~VPMSimpleSpatialResampler();
    virtual WebRtc_Word32 SetTargetFrameSize(WebRtc_UWord32 width,
                                             WebRtc_UWord32 height);
    virtual void SetInputFrameResampleMode(VideoFrameResampling resamplingMode);
    virtual void Reset();
    virtual WebRtc_Word32 ResampleFrame(const VideoFrame& inFrame,
                                        VideoFrame& outFrame);
    virtual WebRtc_UWord32 TargetWidth();
    virtual WebRtc_UWord32 TargetHeight();
    virtual WebRtc_Word32 Release();

private:
    WebRtc_Word32 UpsampleFrame(const VideoFrame& inFrame, VideoFrame& outFrame);
    WebRtc_Word32 CropSize(WebRtc_UWord32 width, WebRtc_UWord32 height,
                           WebRtc_UWord32& croppedWidth,
                           WebRtc_UWord32& croppedHeight) const;
    WebRtc_Word32 SubsampleMultipleOf2(VideoFrame& frame);
    bool ExactMultiplier(WebRtc_UWord32 width, WebRtc_UWord32 height) const;
    WebRtc_Word32 BiLinearInterpolation(const VideoFrame& inFrame,
                                        VideoFrame& outFrame);


    VideoFrameResampling         _resamplingMode;
    WebRtc_UWord32               _targetWidth;
    WebRtc_UWord32               _targetHeight;
    interpolator*                _interpolatorPtr;
};

} //namespace

#endif
