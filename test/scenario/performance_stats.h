/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_SCENARIO_PERFORMANCE_STATS_H_
#define TEST_SCENARIO_PERFORMANCE_STATS_H_

#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/video_frame_buffer.h"
#include "test/statistics.h"

namespace webrtc {
namespace test {

struct VideoFramePair {
  rtc::scoped_refptr<VideoFrameBuffer> captured;
  rtc::scoped_refptr<VideoFrameBuffer> decoded;
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

struct VideoQualityStats {
  int captures_count = 0;
  int valid_count = 0;
  int lost_count = 0;
  Statistics end_to_end_seconds;
  Statistics frame_size;
  Statistics psnr;
};

}  // namespace test
}  // namespace webrtc
#endif  // TEST_SCENARIO_PERFORMANCE_STATS_H_
