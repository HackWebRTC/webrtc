/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "content_metrics_processing.h"
#include "module_common_types.h"
#include "video_coding_defines.h"

#include <math.h>

namespace webrtc {

//////////////////////////////////
/// VCMContentMetricsProcessing //
//////////////////////////////////

VCMContentMetricsProcessing::VCMContentMetricsProcessing():
_frameRate(0),
_recAvgFactor(1 / 150.0f), // matched to  30fps
_frameCntRecursiveAvg(0),
_frameCntUniformAvg(0),
_avgMotionLevel(0.0f),
_avgSpatialLevel(0.0f)
{
    _recursiveAvg = new VideoContentMetrics();
    _uniformAvg = new VideoContentMetrics();
}

VCMContentMetricsProcessing::~VCMContentMetricsProcessing()
{
    delete _recursiveAvg;
    delete _uniformAvg;
}

WebRtc_Word32
VCMContentMetricsProcessing::Reset()
{
    _recursiveAvg->Reset();
    _uniformAvg->Reset();
    _frameRate = 0;
    _frameCntRecursiveAvg = 0;
    _frameCntUniformAvg = 0;
    _avgMotionLevel  = 0.0f;
    _avgSpatialLevel = 0.0f;
    return VCM_OK;
}

void
VCMContentMetricsProcessing::UpdateFrameRate(WebRtc_UWord32 frameRate)
{
    _frameRate = frameRate;
    // Update factor for recursive averaging.
    _recAvgFactor = (float) 1000.0f / ((float)(_frameRate *  kQmMinIntervalMs));

}

VideoContentMetrics*
VCMContentMetricsProcessing::LongTermAvgData()
{
    if (_frameCntRecursiveAvg == 0)
    {
        return NULL;
    }
    return _recursiveAvg;
}

VideoContentMetrics*
VCMContentMetricsProcessing::ShortTermAvgData()
{
    if (_frameCntUniformAvg == 0)
    {
        return NULL;
    }

    // Two metrics are used: motion and spatial level.
    _uniformAvg->motionMagnitudeNZ = _avgMotionLevel /
        (float)(_frameCntUniformAvg);
    _uniformAvg->spatialPredErr = _avgSpatialLevel /
        (float)(_frameCntUniformAvg);

    return _uniformAvg;
}

void
VCMContentMetricsProcessing::ResetShortTermAvgData()
{
     // Reset
    _avgMotionLevel = 0.0f;
    _avgSpatialLevel = 0.0f;
    _frameCntUniformAvg = 0;
}

WebRtc_Word32
VCMContentMetricsProcessing::UpdateContentData(const VideoContentMetrics *contentMetrics)
{
    if (contentMetrics == NULL)
    {
        return VCM_OK;
    }
    return ProcessContent(contentMetrics);

}

WebRtc_UWord32
VCMContentMetricsProcessing::ProcessContent(const VideoContentMetrics *contentMetrics)
{
    // Update the recursive averaged metrics
    // average is over longer window of time: over QmMinIntervalMs ms.
    UpdateRecursiveAvg(contentMetrics);

    // Update the uniform averaged metrics:
    // average is over shorter window of time: based on ~RTCP reports.
    UpdateUniformAvg(contentMetrics);

    return VCM_OK;
}

void
VCMContentMetricsProcessing::UpdateUniformAvg(const VideoContentMetrics *contentMetrics)
{

    // Update frame counter
    _frameCntUniformAvg += 1;

    // Update averaged metrics: motion and spatial level are used.
    _avgMotionLevel += contentMetrics->motionMagnitudeNZ;
    _avgSpatialLevel +=  contentMetrics->spatialPredErr;

    return;

}
void
VCMContentMetricsProcessing::UpdateRecursiveAvg(const VideoContentMetrics *contentMetrics)
{

    // Threshold for size of zero motion cluster:
    // Use for updating 3 motion vector derived metrics:
    // motion magnitude, cluster distortion, and horizontalness.
    float nonZeroMvThr = 0.1f;

    float tmpRecAvgFactor  = _recAvgFactor;

    // Take value as is for first frame (no motion search in frame zero).
    if (_frameCntRecursiveAvg < 1)
    {
        tmpRecAvgFactor = 1;
    }

    _recursiveAvg->motionPredErr = (1 - tmpRecAvgFactor) *
        _recursiveAvg->motionPredErr +
        tmpRecAvgFactor * contentMetrics->motionPredErr;

    _recursiveAvg->sizeZeroMotion = (1 - tmpRecAvgFactor) *
        _recursiveAvg->sizeZeroMotion +
        tmpRecAvgFactor * contentMetrics->sizeZeroMotion;

    _recursiveAvg->spatialPredErr = (1 - tmpRecAvgFactor) *
        _recursiveAvg->spatialPredErr +
        tmpRecAvgFactor * contentMetrics->spatialPredErr;

    _recursiveAvg->spatialPredErrH = (1 - tmpRecAvgFactor) *
        _recursiveAvg->spatialPredErrH +
        tmpRecAvgFactor * contentMetrics->spatialPredErrH;

    _recursiveAvg->spatialPredErrV = (1 - tmpRecAvgFactor) *
        _recursiveAvg->spatialPredErrV +
        tmpRecAvgFactor * contentMetrics->spatialPredErrV;

    // motionMag metric is derived from NFD (normalized frame difference).
    if (kNfdMetric == 1)
    {
        _recursiveAvg->motionMagnitudeNZ = (1 - tmpRecAvgFactor) *
            _recursiveAvg->motionMagnitudeNZ +
            tmpRecAvgFactor * contentMetrics->motionMagnitudeNZ;
    }

    if (contentMetrics->sizeZeroMotion > nonZeroMvThr)
    {
        _recursiveAvg->motionClusterDistortion = (1 - tmpRecAvgFactor) *
            _recursiveAvg->motionClusterDistortion +
            tmpRecAvgFactor *contentMetrics->motionClusterDistortion;

        _recursiveAvg->motionHorizontalness = (1 - _recAvgFactor) *
            _recursiveAvg->motionHorizontalness +
            tmpRecAvgFactor * contentMetrics->motionHorizontalness;

        // motionMag metric is derived from motion vectors.
        if (kNfdMetric == 0)
        {
            _recursiveAvg->motionMagnitudeNZ = (1 - tmpRecAvgFactor) *
                _recursiveAvg->motionMagnitudeNZ +
                tmpRecAvgFactor * contentMetrics->motionMagnitudeNZ;
        }
    }

    // Update native values:
    // TODO (marpan): we don't need to update this every frame.
    _recursiveAvg->nativeHeight = contentMetrics->nativeHeight;
    _recursiveAvg->nativeWidth = contentMetrics->nativeWidth;
    _recursiveAvg->nativeFrameRate = contentMetrics->nativeFrameRate;

    _frameCntRecursiveAvg++;

    return;
}
} //end of namespace
