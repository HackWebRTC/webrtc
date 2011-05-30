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
 * content_analysis.h
 */

#ifndef VPM_CONTENT_ANALYSIS_H
#define VPM_CONTENT_ANALYSIS_H

#include "typedefs.h"
#include "module_common_types.h"

#include "spatial_resampler.h"

namespace webrtc {

class VPMContentAnalysis
{
public:
    VPMContentAnalysis();
    ~VPMContentAnalysis();

    //Initialize ContentAnalysis - should be called prior to extractContentFeature
    //Inputs:	   width, height
    //Return value:   0 if OK, negative value upon error
    WebRtc_Word32 Initialize( WebRtc_UWord16 width,  WebRtc_UWord16 height);
	
    //Extract content Feature - main function of ContentAnalysis
    //Input:		new frame
    //Return value:    pointer to structure containing content Analysis metrics or NULL value upon error
    VideoContentMetrics* ComputeContentMetrics(const VideoFrame* inputFrame);

    //Release all allocated memory
    //Output: 0 if OK, negative value upon error
    WebRtc_Word32 Release();

private:

    //return motion metrics
    VideoContentMetrics* ContentMetrics();

    //Normalized temporal difference metric: for motion magnitude
    WebRtc_Word32 TemporalDiffMetric();

    //Motion metric method: call 2 metrics (magnitude and size)
    WebRtc_Word32 ComputeMotionMetrics();

    //Spatial metric method: computes the 3 frame-average spatial prediction errors (1x2,2x1,2x2)
    WebRtc_Word32 ComputeSpatialMetrics();

    const WebRtc_UWord8*       _origFrame;
    WebRtc_UWord8*             _prevFrame;
    WebRtc_UWord16             _width;
    WebRtc_UWord16             _height;


    //Content Metrics:
    //stores the local average of the metrics
    float                  _motionMagnitudeNZ;  //for motion class
    float                  _spatialPredErr;    //for spatial class
    float                  _spatialPredErrH;   //for spatial class
    float                  _spatialPredErrV;   //for spatial class
    float                  _sizeZeroMotion;     //for motion class
    float                  _motionPredErr;     //for complexity class:
    float                  _motionHorizontalness;    //for coherence class
    float                  _motionClusterDistortion;  //for coherence class

    bool                   _firstFrame;
    bool                   _CAInit;
    VideoContentMetrics*   _cMetrics;

}; // end of VPMContentAnalysis class definition

} // namespace

#endif
