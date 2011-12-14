/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/main/source/tick_time_wrapper.h"

#ifndef WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_MOCK_FAKE_TICK_TIME_WRAPPER_H_
#define WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_MOCK_FAKE_TICK_TIME_WRAPPER_H_

namespace webrtc {

class FakeTickTime : public TickTimeInterface
{
 public:
  FakeTickTime(int64_t start_time_ms)
    : fake_now_(TickTime::Now()) {
    fake_now_ += (MillisecondsToTicks(start_time_ms) - fake_now_.Ticks());
  }
  virtual TickTime Now() const { return fake_now_; }
  virtual int64_t MillisecondTimestamp() const {
    return TicksToMilliseconds(Now().Ticks());
  }
  virtual int64_t MicrosecondTimestamp() const {
    return 1000 * TicksToMilliseconds(Now().Ticks());
  }
  virtual void IncrementDebugClock(int64_t increase_ms) {
    fake_now_ += MillisecondsToTicks(increase_ms);
  }

 private:
  TickTime fake_now_;
};

}  // namespace

#endif  // WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_MOCK_FAKE_TICK_TIME_WRAPPER_H_
