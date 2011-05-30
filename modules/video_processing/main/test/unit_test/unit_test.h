/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef UNIT_TEST_H
#define UNIT_TEST_H

#include "video_processing.h"

#include "vplib.h"

#include <gtest/gtest.h>

using namespace webrtc;

class VideoProcessingModuleTest : public ::testing::Test
{
protected:
    VideoProcessingModuleTest();
    virtual void SetUp();
    virtual void TearDown();
    
    VideoProcessingModule* _vpm;
    FILE* _sourceFile;
    VideoFrame _videoFrame;
    const WebRtc_UWord32 _width;
    const WebRtc_UWord32 _height;
    const WebRtc_UWord32 _frameLength;
};


#endif // UNIT_TEST_H
