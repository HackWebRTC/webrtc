/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_SCENARIO_QUALITY_INFO_H_
#define TEST_SCENARIO_QUALITY_INFO_H_

#include "api/units/timestamp.h"
#include "api/video/video_frame_buffer.h"

namespace webrtc {
namespace test {
struct VideoFramePair {
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> captured;
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> decoded;
  Timestamp capture_time = Timestamp::MinusInfinity();
  Timestamp render_time = Timestamp::PlusInfinity();
  // A unique identifier for the spatial/temporal layer the decoded frame
  // belongs to. Note that this does not reflect the id as defined by the
  // underlying layer setup.
  int layer_id = 0;
  int capture_id = 0;
  int decode_id = 0;
  // Indicates the repeat count for the decoded frame. Meaning that the same
  // decoded frame has matched differend captured frames.
  int repeated = 0;
};
}  // namespace test
}  // namespace webrtc
#endif  // TEST_SCENARIO_QUALITY_INFO_H_
