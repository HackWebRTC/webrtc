/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_COMMON_VIDEO_LIBYUV_TEST_TEST_UTIL_H_
#define WEBRTC_COMMON_VIDEO_LIBYUV_TEST_TEST_UTIL_H_

#include "typedefs.h"

namespace webrtc {

int PrintFrame(const uint8_t* frame, int width, int height);

int PrintFrame(const uint8_t* frame, int width, int height, const char* str);

void CreateImage(int width, int height,
                 uint8_t* frame, int offset,
                 int height_factor, int width_factor);

int ImagePSNRfromBuffer(const uint8_t *ref_frame,
                        const uint8_t *test_frame,
                        int width, int height,
                        double *YPSNRptr);
}  // namespace webrtc
#endif  // WEBRTC_COMMON_VIDEO_LIBYUV_TEST_TEST_UTIL_H_
