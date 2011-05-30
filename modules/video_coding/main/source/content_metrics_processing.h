/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CONTENT_METRICS_PROCESSING_H_
#define WEBRTC_MODULES_VIDEO_CODING_CONTENT_METRICS_PROCESSING_H_

#include "typedefs.h"

namespace webrtc
{

struct VideoContentMetrics;

// QM interval time
enum { kQmMinIntervalMs = 10000 };
enum { kCcMinIntervalMs = 5000 };

//Flag for NFD metric vs motion metric
enum { kNfdMetric  = 1 };

/**********************************/
/* Content Metrics Processing     */
/**********************************/
class VCMContentMetricsProcessing
{
public:
    VCMContentMetricsProcessing();
    ~VCMContentMetricsProcessing();

    // Update class with latest metrics
    WebRtc_Word32 UpdateContentData(const VideoContentMetrics *contentMetrics);

    // Check for content change detection
    bool ContentChangeCheck();

    //Initialize to
    WebRtc_Word32 Reset();

    // Inform class of current frame rate
    void UpdateFrameRate(WebRtc_UWord32 frameRate);

    // Get working (avg) value
    VideoContentMetrics* Data();
private:

    // Compute working avg
    WebRtc_UWord32 ProcessContent(const VideoContentMetrics *contentMetrics);

    // Computation of global metric
    void UpdateGlobalMetric(const VideoContentMetrics *contentMetrics);

    // Compute local average of certain metrics for content change detection
    void UpdateLocalMetricCC(float motionVal, float spatialVal);

    VideoContentMetrics*    _globalRecursiveAvg;
    WebRtc_UWord32          _frameRate;
    float                   _recAvgFactor;
    WebRtc_UWord32          _frameCnt;

    float                   _prevAvgSizeZeroMotion;
    float                   _avgSizeZeroMotion;
    float                   _prevAvgSpatialPredErr;
    float                   _avgSpatialPredErr;
    WebRtc_UWord32          _frameCntForCC;
    WebRtc_UWord64          _lastCCpdateTime;
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_CONTENT_METRICS_PROCESSING_H_
