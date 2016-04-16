/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/libyuv/include/scaler.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_processing/video_denoiser.h"

namespace webrtc {

#if DISPLAY  // Rectangle diagnostics
static void CopyMem8x8(const uint8_t* src,
                       int src_stride,
                       uint8_t* dst,
                       int dst_stride) {
  for (int i = 0; i < 8; i++) {
    memcpy(dst, src, 8);
    src += src_stride;
    dst += dst_stride;
  }
}

static void ShowRect(const std::unique_ptr<DenoiserFilter>& filter,
                     const std::unique_ptr<uint8_t[]>& d_status,
                     const std::unique_ptr<uint8_t[]>& moving_edge_red,
                     const std::unique_ptr<uint8_t[]>& x_density,
                     const std::unique_ptr<uint8_t[]>& y_density,
                     const uint8_t* u_src,
                     const uint8_t* v_src,
                     uint8_t* u_dst,
                     uint8_t* v_dst,
                     int mb_rows_,
                     int mb_cols_,
                     int stride_u_,
                     int stride_v_) {
  for (int mb_row = 0; mb_row < mb_rows_; ++mb_row) {
    for (int mb_col = 0; mb_col < mb_cols_; ++mb_col) {
      int mb_index = mb_row * mb_cols_ + mb_col;
      const uint8_t* mb_src_u =
          u_src + (mb_row << 3) * stride_u_ + (mb_col << 3);
      const uint8_t* mb_src_v =
          v_src + (mb_row << 3) * stride_v_ + (mb_col << 3);
      uint8_t* mb_dst_u = u_dst + (mb_row << 3) * stride_u_ + (mb_col << 3);
      uint8_t* mb_dst_v = v_dst + (mb_row << 3) * stride_v_ + (mb_col << 3);
      uint8_t uv_tmp[8 * 8];
      memset(uv_tmp, 200, 8 * 8);
      if (d_status[mb_index] == 1) {
        // Paint to red.
        CopyMem8x8(mb_src_u, stride_u_, mb_dst_u, stride_u_);
        CopyMem8x8(uv_tmp, 8, mb_dst_v, stride_v_);
      } else if (moving_edge_red[mb_row * mb_cols_ + mb_col] &&
                 x_density[mb_col] * y_density[mb_row]) {
        // Paint to blue.
        CopyMem8x8(uv_tmp, 8, mb_dst_u, stride_u_);
        CopyMem8x8(mb_src_v, stride_v_, mb_dst_v, stride_v_);
      } else {
        CopyMem8x8(mb_src_u, stride_u_, mb_dst_u, stride_u_);
        CopyMem8x8(mb_src_v, stride_v_, mb_dst_v, stride_v_);
      }
    }
  }
}
#endif

VideoDenoiser::VideoDenoiser(bool runtime_cpu_detection)
    : width_(0),
      height_(0),
      filter_(DenoiserFilter::Create(runtime_cpu_detection, &cpu_type_)),
      ne_(new NoiseEstimation()) {}

void VideoDenoiser::DenoiserReset(const VideoFrame& frame,
                                  VideoFrame* denoised_frame,
                                  VideoFrame* denoised_frame_prev) {
  width_ = frame.width();
  height_ = frame.height();
  mb_cols_ = width_ >> 4;
  mb_rows_ = height_ >> 4;
  stride_y_ = frame.stride(kYPlane);
  stride_u_ = frame.stride(kUPlane);
  stride_v_ = frame.stride(kVPlane);

  // Allocate an empty buffer for denoised_frame_prev.
  denoised_frame_prev->CreateEmptyFrame(width_, height_, stride_y_, stride_u_,
                                        stride_v_);
  // Allocate and initialize denoised_frame with key frame.
  denoised_frame->CreateFrame(frame.buffer(kYPlane), frame.buffer(kUPlane),
                              frame.buffer(kVPlane), width_, height_, stride_y_,
                              stride_u_, stride_v_, kVideoRotation_0);
  // Set time parameters to the output frame.
  denoised_frame->set_timestamp(frame.timestamp());
  denoised_frame->set_render_time_ms(frame.render_time_ms());

  // Init noise estimator and allocate buffers.
  ne_->Init(width_, height_, cpu_type_);
  moving_edge_.reset(new uint8_t[mb_cols_ * mb_rows_]);
  mb_filter_decision_.reset(new DenoiserDecision[mb_cols_ * mb_rows_]);
  x_density_.reset(new uint8_t[mb_cols_]);
  y_density_.reset(new uint8_t[mb_rows_]);
  moving_object_.reset(new uint8_t[mb_cols_ * mb_rows_]);
}

int VideoDenoiser::PositionCheck(int mb_row, int mb_col, int noise_level) {
  if (noise_level == 0)
    return 1;
  if ((mb_row <= (mb_rows_ >> 4)) || (mb_col <= (mb_cols_ >> 4)) ||
      (mb_col >= (15 * mb_cols_ >> 4)))
    return 3;
  else if ((mb_row <= (mb_rows_ >> 3)) || (mb_col <= (mb_cols_ >> 3)) ||
           (mb_col >= (7 * mb_cols_ >> 3)))
    return 2;
  else
    return 1;
}

void VideoDenoiser::ReduceFalseDetection(
    const std::unique_ptr<uint8_t[]>& d_status,
    std::unique_ptr<uint8_t[]>* moving_edge_red,
    int noise_level) {
  // From up left corner.
  int mb_col_stop = mb_cols_ - 1;
  for (int mb_row = 0; mb_row <= mb_rows_ - 1; ++mb_row) {
    for (int mb_col = 0; mb_col <= mb_col_stop; ++mb_col) {
      if (d_status[mb_row * mb_cols_ + mb_col]) {
        mb_col_stop = mb_col - 1;
        break;
      }
      (*moving_edge_red)[mb_row * mb_cols_ + mb_col] = 0;
    }
  }
  // From bottom left corner.
  mb_col_stop = mb_cols_ - 1;
  for (int mb_row = mb_rows_ - 1; mb_row >= 0; --mb_row) {
    for (int mb_col = 0; mb_col <= mb_col_stop; ++mb_col) {
      if (d_status[mb_row * mb_cols_ + mb_col]) {
        mb_col_stop = mb_col - 1;
        break;
      }
      (*moving_edge_red)[mb_row * mb_cols_ + mb_col] = 0;
    }
  }
  // From up right corner.
  mb_col_stop = 0;
  for (int mb_row = 0; mb_row <= mb_rows_ - 1; ++mb_row) {
    for (int mb_col = mb_cols_ - 1; mb_col >= mb_col_stop; --mb_col) {
      if (d_status[mb_row * mb_cols_ + mb_col]) {
        mb_col_stop = mb_col + 1;
        break;
      }
      (*moving_edge_red)[mb_row * mb_cols_ + mb_col] = 0;
    }
  }
  // From bottom right corner.
  mb_col_stop = 0;
  for (int mb_row = mb_rows_ - 1; mb_row >= 0; --mb_row) {
    for (int mb_col = mb_cols_ - 1; mb_col >= mb_col_stop; --mb_col) {
      if (d_status[mb_row * mb_cols_ + mb_col]) {
        mb_col_stop = mb_col + 1;
        break;
      }
      (*moving_edge_red)[mb_row * mb_cols_ + mb_col] = 0;
    }
  }
}

bool VideoDenoiser::IsTrailingBlock(const std::unique_ptr<uint8_t[]>& d_status,
                                    int mb_row,
                                    int mb_col) {
  bool ret = false;
  int mb_index = mb_row * mb_cols_ + mb_col;
  if (!mb_row || !mb_col || mb_row == mb_rows_ - 1 || mb_col == mb_cols_ - 1)
    ret = false;
  else
    ret = d_status[mb_index + 1] || d_status[mb_index - 1] ||
          d_status[mb_index + mb_cols_] || d_status[mb_index - mb_cols_];
  return ret;
}

void VideoDenoiser::CopySrcOnMOB(const uint8_t* y_src, uint8_t* y_dst) {
  // Loop over to copy src block if the block is marked as moving object block
  // or if the block may cause trailing artifacts.
  for (int mb_row = 0; mb_row < mb_rows_; ++mb_row) {
    const int mb_index_base = mb_row * mb_cols_;
    const int offset_base = (mb_row << 4) * stride_y_;
    const uint8_t* mb_src_base = y_src + offset_base;
    uint8_t* mb_dst_base = y_dst + offset_base;
    for (int mb_col = 0; mb_col < mb_cols_; ++mb_col) {
      const int mb_index = mb_index_base + mb_col;
      const uint32_t offset_col = mb_col << 4;
      const uint8_t* mb_src = mb_src_base + offset_col;
      uint8_t* mb_dst = mb_dst_base + offset_col;
      // Check if the block is a moving object block or may cause a trailing
      // artifacts.
      if (mb_filter_decision_[mb_index] != FILTER_BLOCK ||
          IsTrailingBlock(moving_edge_, mb_row, mb_col) ||
          (x_density_[mb_col] * y_density_[mb_row] &&
           moving_object_[mb_row * mb_cols_ + mb_col])) {
        // Copy y source.
        filter_->CopyMem16x16(mb_src, stride_y_, mb_dst, stride_y_);
      }
    }
  }
}

void VideoDenoiser::DenoiseFrame(const VideoFrame& frame,
                                 VideoFrame* denoised_frame,
                                 VideoFrame* denoised_frame_prev,
                                 bool noise_estimation_enabled) {
  // If previous width and height are different from current frame's, need to
  // reallocate the buffers and no denoising for the current frame.
  if (width_ != frame.width() || height_ != frame.height()) {
    DenoiserReset(frame, denoised_frame, denoised_frame_prev);
    return;
  }

  // Set buffer pointers.
  const uint8_t* y_src = frame.buffer(kYPlane);
  const uint8_t* u_src = frame.buffer(kUPlane);
  const uint8_t* v_src = frame.buffer(kVPlane);
  uint8_t* y_dst = denoised_frame->buffer(kYPlane);
  uint8_t* u_dst = denoised_frame->buffer(kUPlane);
  uint8_t* v_dst = denoised_frame->buffer(kVPlane);
  uint8_t* y_dst_prev = denoised_frame_prev->buffer(kYPlane);
  memset(x_density_.get(), 0, mb_cols_);
  memset(y_density_.get(), 0, mb_rows_);
  memset(moving_object_.get(), 1, mb_cols_ * mb_rows_);

  uint8_t noise_level = noise_estimation_enabled ? ne_->GetNoiseLevel() : 0;
  int thr_var_base = 16 * 16 * 5;
  // Loop over blocks to accumulate/extract noise level and update x/y_density
  // factors for moving object detection.
  for (int mb_row = 0; mb_row < mb_rows_; ++mb_row) {
    const int mb_index_base = mb_row * mb_cols_;
    const int offset_base = (mb_row << 4) * stride_y_;
    const uint8_t* mb_src_base = y_src + offset_base;
    uint8_t* mb_dst_base = y_dst + offset_base;
    uint8_t* mb_dst_prev_base = y_dst_prev + offset_base;
    for (int mb_col = 0; mb_col < mb_cols_; ++mb_col) {
      const int mb_index = mb_index_base + mb_col;
      const bool ne_enable = (mb_index % NOISE_SUBSAMPLE_INTERVAL == 0);
      const int pos_factor = PositionCheck(mb_row, mb_col, noise_level);
      const uint32_t thr_var_adp = thr_var_base * pos_factor;
      const uint32_t offset_col = mb_col << 4;
      const uint8_t* mb_src = mb_src_base + offset_col;
      uint8_t* mb_dst = mb_dst_base + offset_col;
      uint8_t* mb_dst_prev = mb_dst_prev_base + offset_col;

      // TODO(jackychen): Need SSE2/NEON opt.
      int luma = 0;
      if (ne_enable) {
        for (int i = 4; i < 12; ++i) {
          for (int j = 4; j < 12; ++j) {
            luma += mb_src[i * stride_y_ + j];
          }
        }
      }

      // Get the filtered block and filter_decision.
      mb_filter_decision_[mb_index] =
          filter_->MbDenoise(mb_dst_prev, stride_y_, mb_dst, stride_y_, mb_src,
                             stride_y_, 0, noise_level);

      // If filter decision is FILTER_BLOCK, no need to check moving edge.
      // It is unlikely for a moving edge block to be filtered in current
      // setting.
      if (mb_filter_decision_[mb_index] == FILTER_BLOCK) {
        uint32_t sse_t = 0;
        if (ne_enable) {
          // The variance used in noise estimation is based on the src block in
          // time t (mb_src) and filtered block in time t-1 (mb_dist_prev).
          uint32_t noise_var = filter_->Variance16x8(mb_dst_prev, stride_y_,
                                                     mb_src, stride_y_, &sse_t);
          ne_->GetNoise(mb_index, noise_var, luma);
        }
        moving_edge_[mb_index] = 0;  // Not a moving edge block.
      } else {
        uint32_t sse_t = 0;
        // The variance used in MOD is based on the filtered blocks in time
        // T (mb_dst) and T-1 (mb_dst_prev).
        uint32_t noise_var = filter_->Variance16x8(mb_dst_prev, stride_y_,
                                                   mb_dst, stride_y_, &sse_t);
        if (noise_var > thr_var_adp) {  // Moving edge checking.
          if (ne_enable) {
            ne_->ResetConsecLowVar(mb_index);
          }
          moving_edge_[mb_index] = 1;  // Mark as moving edge block.
          x_density_[mb_col] += (pos_factor < 3);
          y_density_[mb_row] += (pos_factor < 3);
        } else {
          moving_edge_[mb_index] = 0;
          if (ne_enable) {
            // The variance used in noise estimation is based on the src block
            // in time t (mb_src) and filtered block in time t-1 (mb_dist_prev).
            uint32_t noise_var = filter_->Variance16x8(
                mb_dst_prev, stride_y_, mb_src, stride_y_, &sse_t);
            ne_->GetNoise(mb_index, noise_var, luma);
          }
        }
      }
    }  // End of for loop
  }    // End of for loop

  ReduceFalseDetection(moving_edge_, &moving_object_, noise_level);

  CopySrcOnMOB(y_src, y_dst);

  // TODO(jackychen): Need SSE2/NEON opt.
  // Copy u/v planes.
  memcpy(u_dst, u_src, (height_ >> 1) * stride_u_);
  memcpy(v_dst, v_src, (height_ >> 1) * stride_v_);

  // Set time parameters to the output frame.
  denoised_frame->set_timestamp(frame.timestamp());
  denoised_frame->set_render_time_ms(frame.render_time_ms());

#if DISPLAY  // Rectangle diagnostics
  // Show rectangular region
  ShowRect(filter_, moving_edge_, moving_object_, x_density_, y_density_, u_src,
           v_src, u_dst, v_dst, mb_rows_, mb_cols_, stride_u_, stride_v_);
#endif
}

}  // namespace webrtc
