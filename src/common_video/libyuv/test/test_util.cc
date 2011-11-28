/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "common_video/libyuv/test/test_util.h"

#include <math.h>
#include <stdio.h>


namespace webrtc {
int PrintFrame(const uint8_t* frame, int width, int height) {
  if (frame == NULL)
    return -1;
  int k = 0;
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      printf("%d ", frame[k++]);
    }
    printf(" \n");
  }
  printf(" \n");
  return 0;
}

int PrintFrame(const uint8_t* frame, int width,
                int height, const char* str) {
  if (frame == NULL)
     return -1;
  printf("%s %dx%d \n", str, width, height);

  const uint8_t* frame_y = frame;
  const uint8_t* frame_u = frame_y + width * height;
  const uint8_t* frame_v = frame_u + width * height / 4;

  int ret = 0;
  ret += PrintFrame(frame_y, width, height);
  ret += PrintFrame(frame_u, width / 2, height / 2);
  ret += PrintFrame(frame_v, width / 2, height / 2);

  return ret;
}

void CreateImage(int width, int height,
                 uint8_t* frame, int offset,
                 int height_factor, int width_factor) {
  if (frame == NULL)
    return;
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      *frame = static_cast<uint8_t>((i + offset) * height_factor
                                     + j * width_factor);
      frame++;
    }
  }
}

// TODO (mikhal): Following update to latest version, use PSNR tool from libyuv.
int ImagePSNRfromBuffer(const uint8_t* ref_frame,
                        const uint8_t* test_frame,
                        int width, int height, double* YPSNRptr) {
  if (height <= 0 || width <= 0 || ref_frame == NULL || test_frame == NULL)
    return -1;
  // Assumes I420, one frame
  double mse = 0.0;
  double mse_log_sum = 0.0;

  const uint8_t *ref = ref_frame;
  const uint8_t *test = test_frame;
  mse = 0.0;

  // Calculate Y sum-square-difference.
  for ( int k = 0; k < width * height; k++ ) {
    mse += (test[k] - ref[k]) * (test[k] - ref[k]);
  }

  // Divide by number of pixels.
  mse /= static_cast<double> (width * height);

  if (mse == 0) {
    *YPSNRptr = 48;
    return 0;
  }
  // Accumulate for total average
  mse_log_sum += log10(mse);
  *YPSNRptr = 20.0 * log10(255.0) - 10.0 * mse_log_sum;

  return 0;
}
}  // namespace webrtc
