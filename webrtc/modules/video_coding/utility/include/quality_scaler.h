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
  static const int kDefaultLowQpDenominator;
  static const int kDefaultMinDownscaleDimension;
  struct Resolution {
    int width;
    int height;
  };

  QualityScaler();
  void Init(int low_qp_threshold);
  void SetMinResolution(int min_width, int min_height);
  void ReportFramerate(int framerate);
  void ReportQP(int qp);
  void ReportDroppedFrame();
  void Reset(int framerate, int bitrate, int width, int height);
  Resolution GetScaledResolution(const VideoFrame& frame);
  const VideoFrame& GetScaledFrame(const VideoFrame& frame);

 private:
  void AdjustScale(bool up);
  void ClearSamples();

  Scaler scaler_;
  VideoFrame scaled_frame_;

  size_t num_samples_;
  int low_qp_threshold_;
  MovingAverage<int> framedrop_percent_;
  MovingAverage<int> average_qp_;

  int downscale_shift_;
  int min_width_;
  int min_height_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_UTILITY_QUALITY_SCALER_H_
