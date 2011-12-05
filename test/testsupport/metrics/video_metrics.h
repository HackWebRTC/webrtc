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

#include <limits>
#include <vector>

#include "typedefs.h"

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

// PSNR values are filled into the QualityMetricsResult struct.
// If the result is std::numerical_limits<double>::max() the videos were
// equal. Otherwise, PSNR values are in decibel (higher is better). This
// algorithm only compares up to the point when the shortest video ends.
// By definition of PSNR, the result value is undefined if the reference file
// and the test file are identical. In that case the max value for double
// will be set in the result struct.
//
// Returns 0 if successful, negative on errors:
// -1 if the source file cannot be opened
// -2 if the test file cannot be opened
// -3 if any of the files are empty
int PsnrFromFiles(const WebRtc_Word8 *refFileName,
                  const WebRtc_Word8 *testFileName, WebRtc_Word32 width,
                  WebRtc_Word32 height, QualityMetricsResult *result);

// SSIM values are filled into the QualityMetricsResult struct.
// Values range between -1 and 1, where 1 means the files were identical. This
// algorithm only compares up to the point when the shortest video ends.
// By definition, SSIM values varies from -1.0, when everything is different
// between the reference file and the test file, up to 1.0 for two identical
// files.
//
// Returns 0 if successful, negative on errors:
// -1 if the source file cannot be opened
// -2 if the test file cannot be opened
// -3 if any of the files are empty
int SsimFromFiles(const WebRtc_Word8 *refFileName,
                  const WebRtc_Word8 *testFileName, WebRtc_Word32 width,
                  WebRtc_Word32 height, QualityMetricsResult *result);

double SsimFrame(WebRtc_UWord8 *img1, WebRtc_UWord8 *img2,
                 WebRtc_Word32 stride_img1, WebRtc_Word32 stride_img2,
                 WebRtc_Word32 width, WebRtc_Word32 height);

#endif // WEBRTC_MODULES_VIDEO_CODING_TEST_VIDEO_METRICS_H_
