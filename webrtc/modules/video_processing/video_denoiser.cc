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

VideoDenoiser::VideoDenoiser(bool runtime_cpu_detection)
    : width_(0),
      height_(0),
      filter_(DenoiserFilter::Create(runtime_cpu_detection, &cpu_type_)),
      ne_(new NoiseEstimation()) {}

#if EXPERIMENTAL
// Check the mb position(1: close to the center, 3: close to the border).
static int PositionCheck(int mb_row, int mb_col, int mb_rows, int mb_cols) {
  if ((mb_row >= (mb_rows >> 3)) && (mb_row <= (7 * mb_rows >> 3)) &&
      (mb_col >= (mb_cols >> 3)) && (mb_col <= (7 * mb_cols >> 3)))
    return 1;
  else if ((mb_row >= (mb_rows >> 4)) && (mb_row <= (15 * mb_rows >> 4)) &&
           (mb_col >= (mb_cols >> 4)) && (mb_col <= (15 * mb_cols >> 4)))
    return 2;
  else
    return 3;
}

static void ReduceFalseDetection(const std::unique_ptr<uint8_t[]>& d_status,
                                 std::unique_ptr<uint8_t[]>* d_status_tmp1,
                                 std::unique_ptr<uint8_t[]>* d_status_tmp2,
                                 int noise_level,
                                 int mb_rows,
                                 int mb_cols) {
  // Draft. This can be optimized. This code block is to reduce false detection
  // in moving object detection.
  int mb_row_min = noise_level ? mb_rows >> 3 : 1;
  int mb_col_min = noise_level ? mb_cols >> 3 : 1;
  int mb_row_max = noise_level ? (7 * mb_rows >> 3) : mb_rows - 2;
  int mb_col_max = noise_level ? (7 * mb_cols >> 3) : mb_cols - 2;
  memcpy((*d_status_tmp1).get(), d_status.get(), mb_rows * mb_cols);
  // Up left.
  for (int mb_row = mb_row_min; mb_row <= mb_row_max; ++mb_row) {
    for (int mb_col = mb_col_min; mb_col <= mb_col_max; ++mb_col) {
      (*d_status_tmp1)[mb_row * mb_cols + mb_col] |=
          ((*d_status_tmp1)[(mb_row - 1) * mb_cols + mb_col] |
           (*d_status_tmp1)[mb_row * mb_cols + mb_col - 1]);
    }
  }
  memcpy((*d_status_tmp2).get(), (*d_status_tmp1).get(), mb_rows * mb_cols);
  memcpy((*d_status_tmp1).get(), d_status.get(), mb_rows * mb_cols);
  // Bottom left.
  for (int mb_row = mb_row_max; mb_row >= mb_row_min; --mb_row) {
    for (int mb_col = mb_col_min; mb_col <= mb_col_max; ++mb_col) {
      (*d_status_tmp1)[mb_row * mb_cols + mb_col] |=
          ((*d_status_tmp1)[(mb_row + 1) * mb_cols + mb_col] |
           (*d_status_tmp1)[mb_row * mb_cols + mb_col - 1]);
      (*d_status_tmp2)[mb_row * mb_cols + mb_col] &=
          (*d_status_tmp1)[mb_row * mb_cols + mb_col];
    }
  }
  memcpy((*d_status_tmp1).get(), d_status.get(), mb_rows * mb_cols);
  // Up right.
  for (int mb_row = mb_row_min; mb_row <= mb_row_max; ++mb_row) {
    for (int mb_col = mb_col_max; mb_col >= mb_col_min; --mb_col) {
      (*d_status_tmp1)[mb_row * mb_cols + mb_col] |=
          ((*d_status_tmp1)[(mb_row - 1) * mb_cols + mb_col] |
           (*d_status_tmp1)[mb_row * mb_cols + mb_col + 1]);
      (*d_status_tmp2)[mb_row * mb_cols + mb_col] &=
          (*d_status_tmp1)[mb_row * mb_cols + mb_col];
    }
  }
  memcpy((*d_status_tmp1).get(), d_status.get(), mb_rows * mb_cols);
  // Bottom right.
  for (int mb_row = mb_row_max; mb_row >= mb_row_min; --mb_row) {
    for (int mb_col = mb_col_max; mb_col >= mb_col_min; --mb_col) {
      (*d_status_tmp1)[mb_row * mb_cols + mb_col] |=
          ((*d_status_tmp1)[(mb_row + 1) * mb_cols + mb_col] |
           (*d_status_tmp1)[mb_row * mb_cols + mb_col + 1]);
      (*d_status_tmp2)[mb_row * mb_cols + mb_col] &=
          (*d_status_tmp1)[mb_row * mb_cols + mb_col];
    }
  }
}

static bool TrailingBlock(const std::unique_ptr<uint8_t[]>& d_status,
                          int mb_row,
                          int mb_col,
                          int mb_rows,
                          int mb_cols) {
  int mb_index = mb_row * mb_cols + mb_col;
  if (!mb_row || !mb_col || mb_row == mb_rows - 1 || mb_col == mb_cols - 1)
    return false;
  return d_status[mb_index + 1] || d_status[mb_index - 1] ||
         d_status[mb_index + mb_cols] || d_status[mb_index - mb_cols];
}
#endif

#if DISPLAY
void ShowRect(const std::unique_ptr<DenoiserFilter>& filter,
              const std::unique_ptr<uint8_t[]>& d_status,
              const std::unique_ptr<uint8_t[]>& d_status_tmp2,
              const std::unique_ptr<uint8_t[]>& x_density,
              const std::unique_ptr<uint8_t[]>& y_density,
              const uint8_t* u_src,
              const uint8_t* v_src,
              uint8_t* u_dst,
              uint8_t* v_dst,
              int mb_rows,
              int mb_cols,
              int stride_u,
              int stride_v) {
  for (int mb_row = 0; mb_row < mb_rows; ++mb_row) {
    for (int mb_col = 0; mb_col < mb_cols; ++mb_col) {
      int mb_index = mb_row * mb_cols + mb_col;
      const uint8_t* mb_src_u =
          u_src + (mb_row << 3) * stride_u + (mb_col << 3);
      const uint8_t* mb_src_v =
          v_src + (mb_row << 3) * stride_v + (mb_col << 3);
      uint8_t* mb_dst_u = u_dst + (mb_row << 3) * stride_u + (mb_col << 3);
      uint8_t* mb_dst_v = v_dst + (mb_row << 3) * stride_v + (mb_col << 3);
      uint8_t y_tmp_255[8 * 8];
      memset(y_tmp_255, 200, 8 * 8);
      // x_density_[mb_col] * y_density_[mb_row]
      if (d_status[mb_index] == 1) {
        // Paint to red.
        filter->CopyMem8x8(mb_src_u, stride_u, mb_dst_u, stride_u);
        filter->CopyMem8x8(y_tmp_255, 8, mb_dst_v, stride_v);
#if EXPERIMENTAL
      } else if (d_status_tmp2[mb_row * mb_cols + mb_col] &&
                 x_density[mb_col] * y_density[mb_row]) {
#else
      } else if (x_density[mb_col] * y_density[mb_row]) {
#endif
        // Paint to blue.
        filter->CopyMem8x8(y_tmp_255, 8, mb_dst_u, stride_u);
        filter->CopyMem8x8(mb_src_v, stride_v, mb_dst_v, stride_v);
      } else {
        filter->CopyMem8x8(mb_src_u, stride_u, mb_dst_u, stride_u);
        filter->CopyMem8x8(mb_src_v, stride_v, mb_dst_v, stride_v);
      }
    }
  }
}
#endif

void VideoDenoiser::DenoiseFrame(const VideoFrame& frame,
                                 VideoFrame* denoised_frame,
                                 VideoFrame* denoised_frame_prev,
                                 int noise_level_prev) {
  int stride_y = frame.stride(kYPlane);
  int stride_u = frame.stride(kUPlane);
  int stride_v = frame.stride(kVPlane);
  // If previous width and height are different from current frame's, then no
  // denoising for the current frame.
  if (width_ != frame.width() || height_ != frame.height()) {
    width_ = frame.width();
    height_ = frame.height();
    denoised_frame->CreateFrame(frame.buffer(kYPlane), frame.buffer(kUPlane),
                                frame.buffer(kVPlane), width_, height_,
                                stride_y, stride_u, stride_v, kVideoRotation_0);
    denoised_frame_prev->CreateFrame(
        frame.buffer(kYPlane), frame.buffer(kUPlane), frame.buffer(kVPlane),
        width_, height_, stride_y, stride_u, stride_v, kVideoRotation_0);
    // Setting time parameters to the output frame.
    denoised_frame->set_timestamp(frame.timestamp());
    denoised_frame->set_render_time_ms(frame.render_time_ms());
    ne_->Init(width_, height_, cpu_type_);
    return;
  }
  // For 16x16 block.
  int mb_cols = width_ >> 4;
  int mb_rows = height_ >> 4;
  if (metrics_.get() == nullptr)
    metrics_.reset(new DenoiseMetrics[mb_cols * mb_rows]());
  if (d_status_.get() == nullptr) {
    d_status_.reset(new uint8_t[mb_cols * mb_rows]());
#if EXPERIMENTAL
    d_status_tmp1_.reset(new uint8_t[mb_cols * mb_rows]());
    d_status_tmp2_.reset(new uint8_t[mb_cols * mb_rows]());
#endif
    x_density_.reset(new uint8_t[mb_cols]());
    y_density_.reset(new uint8_t[mb_rows]());
  }

  // Denoise on Y plane.
  uint8_t* y_dst = denoised_frame->buffer(kYPlane);
  uint8_t* u_dst = denoised_frame->buffer(kUPlane);
  uint8_t* v_dst = denoised_frame->buffer(kVPlane);
  uint8_t* y_dst_prev = denoised_frame_prev->buffer(kYPlane);
  const uint8_t* y_src = frame.buffer(kYPlane);
  const uint8_t* u_src = frame.buffer(kUPlane);
  const uint8_t* v_src = frame.buffer(kVPlane);
  uint8_t noise_level = noise_level_prev == -1 ? 0 : ne_->GetNoiseLevel();
  // Temporary buffer to store denoising result.
  uint8_t y_tmp[16 * 16] = {0};
  memset(x_density_.get(), 0, mb_cols);
  memset(y_density_.get(), 0, mb_rows);

  // Loop over blocks to accumulate/extract noise level and update x/y_density
  // factors for moving object detection.
  for (int mb_row = 0; mb_row < mb_rows; ++mb_row) {
    for (int mb_col = 0; mb_col < mb_cols; ++mb_col) {
      const uint8_t* mb_src = y_src + (mb_row << 4) * stride_y + (mb_col << 4);
      uint8_t* mb_dst_prev =
          y_dst_prev + (mb_row << 4) * stride_y + (mb_col << 4);
      int mb_index = mb_row * mb_cols + mb_col;
#if EXPERIMENTAL
      int pos_factor = PositionCheck(mb_row, mb_col, mb_rows, mb_cols);
      uint32_t thr_var_adp = 16 * 16 * 5 * (noise_level ? pos_factor : 1);
#else
      uint32_t thr_var_adp = 16 * 16 * 5;
#endif
      int brightness = 0;
      for (int i = 0; i < 16; ++i) {
        for (int j = 0; j < 16; ++j) {
          brightness += mb_src[i * stride_y + j];
        }
      }

      // Get the denoised block.
      filter_->MbDenoise(mb_dst_prev, stride_y, y_tmp, 16, mb_src, stride_y, 0,
                         1, true);
      // The variance is based on the denoised blocks in time T and T-1.
      metrics_[mb_index].var = filter_->Variance16x8(
          mb_dst_prev, stride_y, y_tmp, 16, &metrics_[mb_index].sad);

      if (metrics_[mb_index].var > thr_var_adp) {
        ne_->ResetConsecLowVar(mb_index);
        d_status_[mb_index] = 1;
#if EXPERIMENTAL
        if (noise_level == 0 || pos_factor < 3) {
          x_density_[mb_col] += 1;
          y_density_[mb_row] += 1;
        }
#else
        x_density_[mb_col] += 1;
        y_density_[mb_row] += 1;
#endif
      } else {
        uint32_t sse_t = 0;
        // The variance is based on the src blocks in time T and denoised block
        // in time T-1.
        uint32_t noise_var = filter_->Variance16x8(mb_dst_prev, stride_y,
                                                   mb_src, stride_y, &sse_t);
        ne_->GetNoise(mb_index, noise_var, brightness);
        d_status_[mb_index] = 0;
      }
      // Track denoised frame.
      filter_->CopyMem16x16(y_tmp, 16, mb_dst_prev, stride_y);
    }
  }

#if EXPERIMENTAL
  ReduceFalseDetection(d_status_, &d_status_tmp1_, &d_status_tmp2_, noise_level,
                       mb_rows, mb_cols);
#endif

  // Denoise each MB based on the results of moving objects detection.
  for (int mb_row = 0; mb_row < mb_rows; ++mb_row) {
    for (int mb_col = 0; mb_col < mb_cols; ++mb_col) {
      const uint8_t* mb_src = y_src + (mb_row << 4) * stride_y + (mb_col << 4);
      uint8_t* mb_dst = y_dst + (mb_row << 4) * stride_y + (mb_col << 4);
      const uint8_t* mb_src_u =
          u_src + (mb_row << 3) * stride_u + (mb_col << 3);
      const uint8_t* mb_src_v =
          v_src + (mb_row << 3) * stride_v + (mb_col << 3);
      uint8_t* mb_dst_u = u_dst + (mb_row << 3) * stride_u + (mb_col << 3);
      uint8_t* mb_dst_v = v_dst + (mb_row << 3) * stride_v + (mb_col << 3);
#if EXPERIMENTAL
      if ((!d_status_tmp2_[mb_row * mb_cols + mb_col] ||
           x_density_[mb_col] * y_density_[mb_row] == 0) &&
          !TrailingBlock(d_status_, mb_row, mb_col, mb_rows, mb_cols)) {
#else
      if (x_density_[mb_col] * y_density_[mb_row] == 0) {
#endif
        if (filter_->MbDenoise(mb_dst, stride_y, y_tmp, 16, mb_src, stride_y, 0,
                               noise_level, false) == FILTER_BLOCK) {
          filter_->CopyMem16x16(y_tmp, 16, mb_dst, stride_y);
        } else {
          // Copy y source.
          filter_->CopyMem16x16(mb_src, stride_y, mb_dst, stride_y);
        }
      } else {
        // Copy y source.
        filter_->CopyMem16x16(mb_src, stride_y, mb_dst, stride_y);
      }
      filter_->CopyMem8x8(mb_src_u, stride_u, mb_dst_u, stride_u);
      filter_->CopyMem8x8(mb_src_v, stride_v, mb_dst_v, stride_v);
    }
  }

#if DISPLAY  // Rectangle diagnostics
  // Show rectangular region
  ShowRect(filter_, d_status_, d_status_tmp2_, x_density_, y_density_, u_src,
           v_src, u_dst, v_dst, mb_rows, mb_cols, stride_u, stride_v);
#endif

  // Setting time parameters to the output frame.
  denoised_frame->set_timestamp(frame.timestamp());
  denoised_frame->set_render_time_ms(frame.render_time_ms());
  return;
}

}  // namespace webrtc
