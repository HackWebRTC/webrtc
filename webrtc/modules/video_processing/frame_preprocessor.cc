/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_processing/frame_preprocessor.h"

namespace webrtc {

VPMFramePreprocessor::VPMFramePreprocessor()
    : content_metrics_(nullptr),
      resampled_frame_(),
      enable_ca_(false),
      enable_denoising_(false),
      frame_cnt_(0) {
  spatial_resampler_ = new VPMSimpleSpatialResampler();
  ca_ = new VPMContentAnalysis(true);
  vd_ = new VPMVideoDecimator();
  if (enable_denoising_) {
    denoiser_ = new VideoDenoiser();
  } else {
    denoiser_ = nullptr;
  }
}

VPMFramePreprocessor::~VPMFramePreprocessor() {
  Reset();
  delete ca_;
  delete vd_;
  if (enable_denoising_)
    delete denoiser_;
  delete spatial_resampler_;
}

void  VPMFramePreprocessor::Reset() {
  ca_->Release();
  vd_->Reset();
  content_metrics_ = nullptr;
  spatial_resampler_->Reset();
  enable_ca_ = false;
  frame_cnt_ = 0;
}

void  VPMFramePreprocessor::EnableTemporalDecimation(bool enable) {
  vd_->EnableTemporalDecimation(enable);
}

void VPMFramePreprocessor::EnableContentAnalysis(bool enable) {
  enable_ca_ = enable;
}

void  VPMFramePreprocessor::SetInputFrameResampleMode(
    VideoFrameResampling resampling_mode) {
  spatial_resampler_->SetInputFrameResampleMode(resampling_mode);
}

int32_t VPMFramePreprocessor::SetTargetResolution(
    uint32_t width, uint32_t height, uint32_t frame_rate) {
  if ( (width == 0) || (height == 0) || (frame_rate == 0)) {
    return VPM_PARAMETER_ERROR;
  }
  int32_t ret_val = 0;
  ret_val = spatial_resampler_->SetTargetFrameSize(width, height);

  if (ret_val < 0) return ret_val;

  vd_->SetTargetFramerate(frame_rate);
  return VPM_OK;
}

void VPMFramePreprocessor::SetTargetFramerate(int frame_rate) {
  if (frame_rate == -1) {
    vd_->EnableTemporalDecimation(false);
  } else {
    vd_->EnableTemporalDecimation(true);
    vd_->SetTargetFramerate(frame_rate);
  }
}

void VPMFramePreprocessor::UpdateIncomingframe_rate() {
  vd_->UpdateIncomingframe_rate();
}

uint32_t VPMFramePreprocessor::Decimatedframe_rate() {
  return vd_->Decimatedframe_rate();
}


uint32_t VPMFramePreprocessor::DecimatedWidth() const {
  return spatial_resampler_->TargetWidth();
}


uint32_t VPMFramePreprocessor::DecimatedHeight() const {
  return spatial_resampler_->TargetHeight();
}

int32_t VPMFramePreprocessor::PreprocessFrame(const VideoFrame& frame,
                                              VideoFrame** processed_frame) {
  if (frame.IsZeroSize()) {
    return VPM_PARAMETER_ERROR;
  }

  vd_->UpdateIncomingframe_rate();

  if (vd_->DropFrame()) {
    return 1;  // drop 1 frame
  }

  // Resizing incoming frame if needed. Otherwise, remains nullptr.
  // We are not allowed to resample the input frame (must make a copy of it).
  *processed_frame = nullptr;
  if (denoiser_ != nullptr) {
    denoiser_->DenoiseFrame(frame, &denoised_frame_);
    *processed_frame = &denoised_frame_;
  }

  if (spatial_resampler_->ApplyResample(frame.width(), frame.height()))  {
    int32_t ret;
    if (enable_denoising_) {
      ret = spatial_resampler_->ResampleFrame(denoised_frame_,
                                              &resampled_frame_);
    } else {
      ret = spatial_resampler_->ResampleFrame(frame, &resampled_frame_);
    }
    if (ret != VPM_OK) return ret;
    *processed_frame = &resampled_frame_;
  }

  // Perform content analysis on the frame to be encoded.
  if (enable_ca_) {
    // Compute new metrics every |kSkipFramesCA| frames, starting with
    // the first frame.
    if (frame_cnt_ % kSkipFrameCA == 0) {
      if (*processed_frame == nullptr)  {
        content_metrics_ = ca_->ComputeContentMetrics(frame);
      } else {
        content_metrics_ = ca_->ComputeContentMetrics(**processed_frame);
      }
    }
  }
  ++frame_cnt_;
  return VPM_OK;
}

VideoContentMetrics* VPMFramePreprocessor::ContentMetrics() const {
  return content_metrics_;
}

}  // namespace
