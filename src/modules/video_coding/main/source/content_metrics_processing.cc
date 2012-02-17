/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
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
_recAvgFactor(1 / 150.0f), // matched to  30fps
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
    _frameCntUniformAvg = 0;
    _avgMotionLevel  = 0.0f;
    _avgSpatialLevel = 0.0f;
    return VCM_OK;
}

void
VCMContentMetricsProcessing::UpdateFrameRate(WebRtc_UWord32 frameRate)
{
    // Update factor for recursive averaging.
    _recAvgFactor = (float) 1000.0f / ((float)(frameRate *  kQmMinIntervalMs));

}

VideoContentMetrics*
VCMContentMetricsProcessing::LongTermAvgData()
{
    return _recursiveAvg;
}

VideoContentMetrics*
VCMContentMetricsProcessing::ShortTermAvgData()
{
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
VCMContentMetricsProcessing::UpdateContentData(
    const VideoContentMetrics *contentMetrics)
{
    if (contentMetrics == NULL)
    {
        return VCM_OK;
    }
    return ProcessContent(contentMetrics);

}

WebRtc_UWord32
VCMContentMetricsProcessing::ProcessContent(
    const VideoContentMetrics *contentMetrics)
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
VCMContentMetricsProcessing::UpdateUniformAvg(
    const VideoContentMetrics *contentMetrics)
{
    // Update frame counter
    _frameCntUniformAvg += 1;

    // Update averaged metrics: motion and spatial level are used.
    _avgMotionLevel += contentMetrics->motionMagnitudeNZ;
    _avgSpatialLevel +=  contentMetrics->spatialPredErr;

    return;

}
void VCMContentMetricsProcessing::UpdateRecursiveAvg(
    const VideoContentMetrics *contentMetrics) {

  // Spatial metrics: 2x2, 1x2(H), 2x1(V).
  _recursiveAvg->spatialPredErr = (1 - _recAvgFactor) *
      _recursiveAvg->spatialPredErr +
      _recAvgFactor * contentMetrics->spatialPredErr;

  _recursiveAvg->spatialPredErrH = (1 - _recAvgFactor) *
      _recursiveAvg->spatialPredErrH +
      _recAvgFactor * contentMetrics->spatialPredErrH;

  _recursiveAvg->spatialPredErrV = (1 - _recAvgFactor) *
      _recursiveAvg->spatialPredErrV +
      _recAvgFactor * contentMetrics->spatialPredErrV;

  // Motion metric: Derived from NFD (normalized frame difference)
  _recursiveAvg->motionMagnitudeNZ = (1 - _recAvgFactor) *
      _recursiveAvg->motionMagnitudeNZ +
      _recAvgFactor * contentMetrics->motionMagnitudeNZ;

  return;
}
} //end of namespace
