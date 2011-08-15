/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "unit_test.h"
#include "video_processing.h"
#include "vplib.h"
#include "content_analysis.h"

using namespace webrtc;

TEST_F(VideoProcessingModuleTest, ContentAnalysis)
{
    VPMContentAnalysis    _ca_c(false);
    VPMContentAnalysis    _ca_sse;
    VideoContentMetrics  *_cM_c, *_cM_SSE;

    _ca_c.Initialize(_width,_height);
    _ca_sse.Initialize(_width,_height);

    ASSERT_EQ(352, _width);
    ASSERT_EQ(288, _height);

    while (fread(_videoFrame.Buffer(), 1, _frameLength, _sourceFile)
           == _frameLength)
    {
        _cM_c   = _ca_c.ComputeContentMetrics(&_videoFrame);
        _cM_SSE = _ca_sse.ComputeContentMetrics(&_videoFrame);

        ASSERT_EQ(_cM_c->spatialPredErr,    _cM_SSE->spatialPredErr);
        ASSERT_EQ(_cM_c->spatialPredErrV,   _cM_SSE->spatialPredErrV);
        ASSERT_EQ(_cM_c->spatialPredErrH,   _cM_SSE->spatialPredErrH);
        ASSERT_EQ(_cM_c->motionMagnitudeNZ, _cM_SSE->motionMagnitudeNZ);
    }
    ASSERT_NE(0, feof(_sourceFile)) << "Error reading source file";
}
