/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_PROCESSING_UTIL_NOISE_ESTIMATION_H_
#define WEBRTC_MODULES_VIDEO_PROCESSING_UTIL_NOISE_ESTIMATION_H_

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/video_processing/include/video_processing_defines.h"
#include "webrtc/modules/video_processing/util/denoiser_filter.h"

namespace webrtc {

#define EXPERIMENTAL 0
#define DISPLAY 0

const int kNoiseThreshold = 200;
const int kNoiseThresholdNeon = 70;
const int kConsecLowVarFrame = 6;
const int kAverageLumaMin = 20;
const int kAverageLumaMax = 220;
const int kBlockSelectionVarMax = kNoiseThreshold << 1;

class NoiseEstimation {
 public:
  void Init(int width, int height, CpuType cpu_type);
  void GetNoise(int mb_index, uint32_t var, uint32_t luma);
  void ResetConsecLowVar(int mb_index);
  void UpdateNoiseLevel();
  // 0: low noise, 1: high noise
  uint8_t GetNoiseLevel();

 private:
  int width_;
  int height_;
  int mb_rows_;
  int mb_cols_;
  CpuType cpu_type_;
  uint32_t noise_var_;
  double noise_var_accum_;
  int num_noisy_block_;
  int num_static_block_;
  double percent_static_block_;
  rtc::scoped_ptr<uint32_t[]> consec_low_var_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_PROCESSING_UTIL_NOISE_ESTIMATION_H_
