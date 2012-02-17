/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
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

// QM interval time (in ms)
enum { kQmMinIntervalMs = 10000 };

// Flag for NFD metric vs motion metric
enum { kNfdMetric = 1 };

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

    // Reset the short-term averaged content data
     void ResetShortTermAvgData();

    // Initialize to
    WebRtc_Word32 Reset();

    // Inform class of current frame rate
    void UpdateFrameRate(WebRtc_UWord32 frameRate);

    // Returns the long-term averaged content data:
    // recursive average over longer time scale
    VideoContentMetrics* LongTermAvgData();

    // Returns the short-term averaged content data:
    // uniform average over shorter time scale
     VideoContentMetrics* ShortTermAvgData();
private:

    // Compute working avg
    WebRtc_UWord32 ProcessContent(const VideoContentMetrics *contentMetrics);

    // Update the recursive averaged metrics: longer time average (~5/10 secs).
    void UpdateRecursiveAvg(const VideoContentMetrics *contentMetrics);

    // Update the uniform averaged metrics: shorter time average (~RTCP report).
    void UpdateUniformAvg(const VideoContentMetrics *contentMetrics);

    VideoContentMetrics*    _recursiveAvg;
    VideoContentMetrics*    _uniformAvg;
    float                   _recAvgFactor;
    WebRtc_UWord32          _frameCntUniformAvg;
    float                   _avgMotionLevel;
    float                   _avgSpatialLevel;
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_CONTENT_METRICS_PROCESSING_H_
