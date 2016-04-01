/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_PROCESSING_VIDEO_DENOISER_H_
#define WEBRTC_MODULES_VIDEO_PROCESSING_VIDEO_DENOISER_H_

#include <memory>

#include "webrtc/modules/video_processing/util/denoiser_filter.h"
#include "webrtc/modules/video_processing/util/noise_estimation.h"
#include "webrtc/modules/video_processing/util/skin_detection.h"

namespace webrtc {

class VideoDenoiser {
 public:
  explicit VideoDenoiser(bool runtime_cpu_detection);
  void DenoiseFrame(const VideoFrame& frame,
                    VideoFrame* denoised_frame,
                    VideoFrame* denoised_frame_track,
                    int noise_level_prev);

 private:
  int width_;
  int height_;
  CpuType cpu_type_;
  std::unique_ptr<DenoiseMetrics[]> metrics_;
  std::unique_ptr<DenoiserFilter> filter_;
  std::unique_ptr<NoiseEstimation> ne_;
  std::unique_ptr<uint8_t[]> d_status_;
#if EXPERIMENTAL
  std::unique_ptr<uint8_t[]> d_status_tmp1_;
  std::unique_ptr<uint8_t[]> d_status_tmp2_;
#endif
  std::unique_ptr<uint8_t[]> x_density_;
  std::unique_ptr<uint8_t[]> y_density_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_PROCESSING_VIDEO_DENOISER_H_
