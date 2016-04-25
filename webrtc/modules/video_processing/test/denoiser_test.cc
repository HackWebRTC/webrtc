/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include <memory>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_processing/include/video_processing.h"
#include "webrtc/modules/video_processing/test/video_processing_unittest.h"
#include "webrtc/modules/video_processing/video_denoiser.h"
#include "webrtc/test/frame_utils.h"

namespace webrtc {

TEST_F(VideoProcessingTest, CopyMem) {
  std::unique_ptr<DenoiserFilter> df_c(DenoiserFilter::Create(false, nullptr));
  std::unique_ptr<DenoiserFilter> df_sse_neon(
      DenoiserFilter::Create(true, nullptr));
  uint8_t src[16 * 16], dst[16 * 16];
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < 16; ++j) {
      src[i * 16 + j] = i * 16 + j;
    }
  }

  memset(dst, 0, 16 * 16);
  df_c->CopyMem16x16(src, 16, dst, 16);
  EXPECT_EQ(0, memcmp(src, dst, 16 * 16));

  memset(dst, 0, 16 * 16);
  df_sse_neon->CopyMem16x16(src, 16, dst, 16);
  EXPECT_EQ(0, memcmp(src, dst, 16 * 16));
}

TEST_F(VideoProcessingTest, Variance) {
  std::unique_ptr<DenoiserFilter> df_c(DenoiserFilter::Create(false, nullptr));
  std::unique_ptr<DenoiserFilter> df_sse_neon(
      DenoiserFilter::Create(true, nullptr));
  uint8_t src[16 * 16], dst[16 * 16];
  uint32_t sum = 0, sse = 0, var;
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < 16; ++j) {
      src[i * 16 + j] = i * 16 + j;
    }
  }
  // Compute the 16x8 variance of the 16x16 block.
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 16; ++j) {
      sum += (i * 32 + j);
      sse += (i * 32 + j) * (i * 32 + j);
    }
  }
  var = sse - ((sum * sum) >> 7);
  memset(dst, 0, 16 * 16);
  EXPECT_EQ(var, df_c->Variance16x8(src, 16, dst, 16, &sse));
  EXPECT_EQ(var, df_sse_neon->Variance16x8(src, 16, dst, 16, &sse));
}

TEST_F(VideoProcessingTest, MbDenoise) {
  std::unique_ptr<DenoiserFilter> df_c(DenoiserFilter::Create(false, nullptr));
  std::unique_ptr<DenoiserFilter> df_sse_neon(
      DenoiserFilter::Create(true, nullptr));
  uint8_t running_src[16 * 16], src[16 * 16];
  uint8_t dst[16 * 16], dst_sse_neon[16 * 16];

  // Test case: |diff| <= |3 + shift_inc1|
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < 16; ++j) {
      running_src[i * 16 + j] = i * 11 + j;
      src[i * 16 + j] = i * 11 + j + 2;
    }
  }
  memset(dst, 0, 16 * 16);
  df_c->MbDenoise(running_src, 16, dst, 16, src, 16, 0, 1);
  memset(dst_sse_neon, 0, 16 * 16);
  df_sse_neon->MbDenoise(running_src, 16, dst_sse_neon, 16, src, 16, 0, 1);
  EXPECT_EQ(0, memcmp(dst, dst_sse_neon, 16 * 16));

  // Test case: |diff| >= |4 + shift_inc1|
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < 16; ++j) {
      running_src[i * 16 + j] = i * 11 + j;
      src[i * 16 + j] = i * 11 + j + 5;
    }
  }
  memset(dst, 0, 16 * 16);
  df_c->MbDenoise(running_src, 16, dst, 16, src, 16, 0, 1);
  memset(dst_sse_neon, 0, 16 * 16);
  df_sse_neon->MbDenoise(running_src, 16, dst_sse_neon, 16, src, 16, 0, 1);
  EXPECT_EQ(0, memcmp(dst, dst_sse_neon, 16 * 16));

  // Test case: |diff| >= 8
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < 16; ++j) {
      running_src[i * 16 + j] = i * 11 + j;
      src[i * 16 + j] = i * 11 + j + 8;
    }
  }
  memset(dst, 0, 16 * 16);
  df_c->MbDenoise(running_src, 16, dst, 16, src, 16, 0, 1);
  memset(dst_sse_neon, 0, 16 * 16);
  df_sse_neon->MbDenoise(running_src, 16, dst_sse_neon, 16, src, 16, 0, 1);
  EXPECT_EQ(0, memcmp(dst, dst_sse_neon, 16 * 16));

  // Test case: |diff| > 15
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < 16; ++j) {
      running_src[i * 16 + j] = i * 11 + j;
      src[i * 16 + j] = i * 11 + j + 16;
    }
  }
  memset(dst, 0, 16 * 16);
  DenoiserDecision decision =
      df_c->MbDenoise(running_src, 16, dst, 16, src, 16, 0, 1);
  EXPECT_EQ(COPY_BLOCK, decision);
  decision = df_sse_neon->MbDenoise(running_src, 16, dst, 16, src, 16, 0, 1);
  EXPECT_EQ(COPY_BLOCK, decision);
}

TEST_F(VideoProcessingTest, Denoiser) {
  // Used in swap buffer.
  int denoised_frame_toggle = 0;
  // Create pure C denoiser.
  VideoDenoiser denoiser_c(false);
  // Create SSE or NEON denoiser.
  VideoDenoiser denoiser_sse_neon(true);
  VideoFrame denoised_frame_c;
  VideoFrame denoised_frame_prev_c;
  VideoFrame denoised_frame_sse_neon;
  VideoFrame denoised_frame_prev_sse_neon;

  std::unique_ptr<uint8_t[]> video_buffer(new uint8_t[frame_length_]);
  while (fread(video_buffer.get(), 1, frame_length_, source_file_) ==
         frame_length_) {
    // Using ConvertToI420 to add stride to the image.
    EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0, width_, height_,
                               0, kVideoRotation_0, &video_frame_));

    VideoFrame* p_denoised_c = &denoised_frame_c;
    VideoFrame* p_denoised_prev_c = &denoised_frame_prev_c;
    VideoFrame* p_denoised_sse_neon = &denoised_frame_sse_neon;
    VideoFrame* p_denoised_prev_sse_neon = &denoised_frame_prev_sse_neon;
    // Swap the buffer to save one memcpy in DenoiseFrame.
    if (denoised_frame_toggle) {
      p_denoised_c = &denoised_frame_prev_c;
      p_denoised_prev_c = &denoised_frame_c;
      p_denoised_sse_neon = &denoised_frame_prev_sse_neon;
      p_denoised_prev_sse_neon = &denoised_frame_sse_neon;
    }
    denoiser_c.DenoiseFrame(video_frame_, p_denoised_c, p_denoised_prev_c,
                            false);
    denoiser_sse_neon.DenoiseFrame(video_frame_, p_denoised_sse_neon,
                                   p_denoised_prev_sse_neon, false);
    // Invert the flag.
    denoised_frame_toggle ^= 1;
    // Denoising results should be the same for C and SSE/NEON denoiser.
    ASSERT_TRUE(test::FramesEqual(*p_denoised_c, *p_denoised_sse_neon));
  }
  ASSERT_NE(0, feof(source_file_)) << "Error reading source file";
}

}  // namespace webrtc
