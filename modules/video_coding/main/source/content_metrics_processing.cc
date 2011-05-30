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
#include "tick_time.h"
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
_frameCnt(0),
_prevAvgSizeZeroMotion(0),
_avgSizeZeroMotion(0),
_prevAvgSpatialPredErr(0),
_avgSpatialPredErr(0),
_frameCntForCC(0),
_lastCCpdateTime(0)
{
    _globalRecursiveAvg = new VideoContentMetrics();
}

VCMContentMetricsProcessing::~VCMContentMetricsProcessing()
{
    delete _globalRecursiveAvg;
}

WebRtc_Word32
VCMContentMetricsProcessing::Reset()
{
    _globalRecursiveAvg->Reset();
    _frameCnt = 0;
    _frameRate = 0;
    //_recAvgFactor = 1 / 150.0f; // matched to 30 fps
    _prevAvgSizeZeroMotion = 0;
    _avgSizeZeroMotion = 0;
    _prevAvgSpatialPredErr = 0;
    _avgSpatialPredErr = 0;
    _frameCntForCC = 0;

    return VCM_OK;
}
void
VCMContentMetricsProcessing::UpdateFrameRate(WebRtc_UWord32 frameRate)
{
    _frameRate = frameRate;
    //Update recursive avg factor
    _recAvgFactor = (float) 1000 / ((float)(_frameRate *  kQmMinIntervalMs));

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

VideoContentMetrics*
VCMContentMetricsProcessing::Data()
{
    if (_frameCnt == 0)
    {
        return NULL;
    }
    return _globalRecursiveAvg;
}

WebRtc_UWord32
VCMContentMetricsProcessing::ProcessContent(const VideoContentMetrics *contentMetrics)
{
    // update global metric
    UpdateGlobalMetric(contentMetrics);

    //Update metrics over local window for content change (CC) detection:
    //two metrics are used for CC detection: size of zero motion, and spatial prediction error
    //Not currently used:
    //UpdateLocalMetricCC(contentMetrics->sizeZeroMotion, contentMetrics->spatialPredErr);

    return VCM_OK;
}

bool
VCMContentMetricsProcessing::ContentChangeCheck()
{
     bool result = false;

    // Thresholds for bitrate and content change detection
    float qmContentChangePercMotion = 0.4f;
    float qmContentChangePercSpatial = 0.4f;

    WebRtc_Word64 now = VCMTickTime::MillisecondTimestamp();
    if ( (now - _lastCCpdateTime) < kCcMinIntervalMs)
    {
        //keep averaging
        return result;
    }
    else //check for detection and reset
    {
        //normalize
        _avgSizeZeroMotion = _avgSizeZeroMotion / (float)(_frameCntForCC);
        _prevAvgSpatialPredErr = _prevAvgSpatialPredErr / (float)(_frameCntForCC);

        //check for content change
        float diffMotion = fabs(_avgSizeZeroMotion - _prevAvgSizeZeroMotion);
        float diffSpatial = fabs(_avgSpatialPredErr -_prevAvgSpatialPredErr);
        if ((diffMotion > (_avgSizeZeroMotion * qmContentChangePercMotion)) ||
            (diffSpatial > (_prevAvgSpatialPredErr * qmContentChangePercSpatial)))
        {
            result = true;
        }

        //copy to previous
        _prevAvgSizeZeroMotion = _avgSizeZeroMotion;
        _prevAvgSpatialPredErr = _avgSpatialPredErr;

        //reset
        _avgSizeZeroMotion = 0.;
        _avgSpatialPredErr = 0.;
        _frameCntForCC = 0;

         _lastCCpdateTime = now;

    }

    return result;
}

//update metrics for content change detection: update is uniform average over soem time window
void VCMContentMetricsProcessing::UpdateLocalMetricCC(float motionVal, float spatialVal)
{

    _frameCntForCC += 1;
    _avgSizeZeroMotion += motionVal;
    _avgSpatialPredErr += spatialVal;

    return;

}
void
VCMContentMetricsProcessing::UpdateGlobalMetric(const VideoContentMetrics *contentMetrics)
{

    // Threshold for size of zero motion cluster: for updating 3 metrics:
    // motion magnitude, cluster distortion, and horizontalness
    float nonZeroMvThr = 0.1f;

    // first zero and one: take value as is (no motion search in frame zero).
    float tmpRecAvgFactor  = _recAvgFactor;
    if (_frameCnt < 1)
    {
        _recAvgFactor = 1;
    }

    _globalRecursiveAvg->motionPredErr = (1 - _recAvgFactor) * _globalRecursiveAvg->motionPredErr +
                                          _recAvgFactor * contentMetrics->motionPredErr;

    _globalRecursiveAvg->sizeZeroMotion = (1 - _recAvgFactor) * _globalRecursiveAvg->sizeZeroMotion +
                                           _recAvgFactor * contentMetrics->sizeZeroMotion;

    _globalRecursiveAvg->spatialPredErr = (1 - _recAvgFactor) * _globalRecursiveAvg->spatialPredErr +
                                           _recAvgFactor * contentMetrics->spatialPredErr;

    _globalRecursiveAvg->spatialPredErrH = (1 - _recAvgFactor) * _globalRecursiveAvg->spatialPredErrH +
                                           _recAvgFactor * contentMetrics->spatialPredErrH;

    _globalRecursiveAvg->spatialPredErrV = (1 - _recAvgFactor) * _globalRecursiveAvg->spatialPredErrV +
                                           _recAvgFactor * contentMetrics->spatialPredErrV;

    //motionMag metric is derived from NFD (normalized frame difference)
    if (kNfdMetric == 1)
    {
        _globalRecursiveAvg->motionMagnitudeNZ = (1 - _recAvgFactor) * _globalRecursiveAvg->motionMagnitudeNZ +
                                                    _recAvgFactor * contentMetrics->motionMagnitudeNZ;
    }

    if (contentMetrics->sizeZeroMotion > nonZeroMvThr)
    {
        _globalRecursiveAvg->motionClusterDistortion = (1 - _recAvgFactor) * _globalRecursiveAvg->motionClusterDistortion +
                                                    _recAvgFactor *contentMetrics->motionClusterDistortion;

        _globalRecursiveAvg->motionHorizontalness = (1 - _recAvgFactor) * _globalRecursiveAvg->motionHorizontalness +
                                                _recAvgFactor * contentMetrics->motionHorizontalness;

        //motionMag metric is derived from motion vectors
        if (kNfdMetric == 0)
        {
            _globalRecursiveAvg->motionMagnitudeNZ = (1 - _recAvgFactor) * _globalRecursiveAvg->motionMagnitudeNZ +
                                                _recAvgFactor * contentMetrics->motionMagnitudeNZ;
        }
    }

    // update native values:
    _globalRecursiveAvg->nativeHeight = contentMetrics->nativeHeight;
    _globalRecursiveAvg->nativeWidth = contentMetrics->nativeWidth;
    _globalRecursiveAvg->nativeFrameRate = contentMetrics->nativeFrameRate;

    if (_frameCnt < 1)
    {
        _recAvgFactor = tmpRecAvgFactor;
    }
    _frameCnt++;
    return;
}

}
