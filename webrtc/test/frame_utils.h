/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_TEST_FRAME_UTILS_H_
#define WEBRTC_TEST_FRAME_UTILS_H_

#include "webrtc/base/basictypes.h"

namespace webrtc {
class VideoFrame;
namespace test {

bool EqualPlane(const uint8_t* data1,
                const uint8_t* data2,
                int stride,
                int width,
                int height);

bool FramesEqual(const webrtc::VideoFrame& f1, const webrtc::VideoFrame& f2);

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_FRAME_UTILS_H_
