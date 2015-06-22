/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/video_coding/utility/include/quality_scaler.h"

namespace webrtc {

static const int kMinFps = 10;
static const int kMeasureSeconds = 5;
static const int kFramedropPercentThreshold = 60;

const int QualityScaler::kDefaultLowQpDenominator = 3;
// Note that this is the same for width and height to permit 120x90 in both
// portrait and landscape mode.
const int QualityScaler::kDefaultMinDownscaleDimension = 90;

QualityScaler::QualityScaler()
    : num_samples_(0),
      low_qp_threshold_(-1),
      downscale_shift_(0),
      min_width_(kDefaultMinDownscaleDimension),
      min_height_(kDefaultMinDownscaleDimension) {
}

void QualityScaler::Init(int low_qp_threshold) {
  ClearSamples();
  low_qp_threshold_ = low_qp_threshold;
}

void QualityScaler::SetMinResolution(int min_width, int min_height) {
  min_width_ = min_width;
  min_height_ = min_height;
}

// Report framerate(fps) to estimate # of samples.
void QualityScaler::ReportFramerate(int framerate) {
  num_samples_ = static_cast<size_t>(
      kMeasureSeconds * (framerate < kMinFps ? kMinFps : framerate));
}

void QualityScaler::ReportQP(int qp) {
  framedrop_percent_.AddSample(0);
  average_qp_.AddSample(qp);
}

void QualityScaler::ReportDroppedFrame() {
  framedrop_percent_.AddSample(100);
}

QualityScaler::Resolution QualityScaler::GetScaledResolution(
    const VideoFrame& frame) {
  // Should be set through InitEncode -> Should be set by now.
  assert(low_qp_threshold_ >= 0);
  assert(num_samples_ > 0);

  Resolution res;
  res.width = frame.width();
  res.height = frame.height();

  // Update scale factor.
  int avg_drop = 0;
  int avg_qp = 0;
  if (framedrop_percent_.GetAverage(num_samples_, &avg_drop) &&
      avg_drop >= kFramedropPercentThreshold) {
    AdjustScale(false);
  } else if (average_qp_.GetAverage(num_samples_, &avg_qp) &&
      avg_qp <= low_qp_threshold_) {
    AdjustScale(true);
  }

  assert(downscale_shift_ >= 0);
  for (int shift = downscale_shift_;
       shift > 0 && (res.width >> 1 >= min_width_) &&
           (res.height >> 1 >= min_height_);
       --shift) {
    res.width >>= 1;
    res.height >>= 1;
  }

  return res;
}

const VideoFrame& QualityScaler::GetScaledFrame(const VideoFrame& frame) {
  Resolution res = GetScaledResolution(frame);
  if (res.width == frame.width())
    return frame;

  scaler_.Set(frame.width(),
              frame.height(),
              res.width,
              res.height,
              kI420,
              kI420,
              kScaleBox);
  if (scaler_.Scale(frame, &scaled_frame_) != 0)
    return frame;

  scaled_frame_.set_ntp_time_ms(frame.ntp_time_ms());
  scaled_frame_.set_timestamp(frame.timestamp());
  scaled_frame_.set_render_time_ms(frame.render_time_ms());

  return scaled_frame_;
}

void QualityScaler::ClearSamples() {
  framedrop_percent_.Reset();
  average_qp_.Reset();
}

void QualityScaler::AdjustScale(bool up) {
  downscale_shift_ += up ? -1 : 1;
  if (downscale_shift_ < 0)
    downscale_shift_ = 0;
  ClearSamples();
}

}  // namespace webrtc
