/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_UTILITY_QUALITY_SCALER_H_
#define WEBRTC_MODULES_VIDEO_CODING_UTILITY_QUALITY_SCALER_H_

#include "webrtc/common_video/libyuv/include/scaler.h"
#include "webrtc/modules/video_coding/utility/include/moving_average.h"

namespace webrtc {
class QualityScaler {
 public:
  struct Resolution {
    int width;
    int height;
  };

  QualityScaler();
  void Init(int max_qp);
  void SetMinResolution(int min_width, int min_height);
  void ReportFramerate(int framerate);

  // Report QP for SW encoder, report framesize fluctuation for HW encoder,
  // only one of these two functions should be called, framesize fluctuation
  // is to be used only if qp isn't available.
  void ReportNormalizedQP(int qp);
  void ReportNormalizedFrameSizeFluctuation(double framesize_deviation);
  void ReportDroppedFrame();
  void Reset(int framerate, int bitrate, int width, int height);
  Resolution GetScaledResolution(const I420VideoFrame& frame);
  const I420VideoFrame& GetScaledFrame(const I420VideoFrame& frame);

 private:
  void AdjustScale(bool up);
  void ClearSamples();

  Scaler scaler_;
  I420VideoFrame scaled_frame_;

  size_t num_samples_;
  int target_framesize_;
  int low_qp_threshold_;
  MovingAverage<int> framedrop_percent_;
  MovingAverage<double> frame_quality_;

  int downscale_shift_;
  int min_width_;
  int min_height_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_UTILITY_QUALITY_SCALER_H_
