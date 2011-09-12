/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef WEBRTC_MODULES_VIDEO_CODING_TEST_VIDEO_METRICS_H_
#define WEBRTC_MODULES_VIDEO_CODING_TEST_VIDEO_METRICS_H_

#include "typedefs.h"
#include <limits>
#include <vector>


// Contains video quality metrics result for a single frame.
struct FrameResult {
  WebRtc_Word32 frame_number;
  double value;
};

// Result from a PSNR/SSIM calculation operation.
// The frames in this data structure are 0-indexed.
struct QualityMetricsResult {
  QualityMetricsResult() :
    average(0.0),
    min(std::numeric_limits<double>::max()),
    max(std::numeric_limits<double>::min()),
    min_frame_number(-1),
    max_frame_number(-1)
  {};
  double average;
  double min;
  double max;
  WebRtc_Word32 min_frame_number;
  WebRtc_Word32 max_frame_number;
  std::vector<FrameResult> frames;
};

// PSNR & SSIM calculations
WebRtc_Word32
PsnrFromFiles(const WebRtc_Word8 *refFileName,
        const WebRtc_Word8 *testFileName, WebRtc_Word32 width,
        WebRtc_Word32 height, QualityMetricsResult *result);

WebRtc_Word32
SsimFromFiles(const WebRtc_Word8 *refFileName,
        const WebRtc_Word8 *testFileName, WebRtc_Word32 width,
        WebRtc_Word32 height, QualityMetricsResult *result);


#endif // WEBRTC_MODULES_VIDEO_CODING_TEST_VIDEO_METRICS_H_
