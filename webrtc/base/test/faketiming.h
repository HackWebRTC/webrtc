/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_TEST_FAKETIMING_H_
#define WEBRTC_BASE_TEST_FAKETIMING_H_

#include "webrtc/base/timing.h"
#include "webrtc/base/timeutils.h"

namespace rtc {

class FakeTiming : public Timing {
 public:
  // Starts at Jan 1, 1983 (UTC).
  FakeTiming() : now_(410227200.0) {}
  double TimerNow() override { return now_; }
  void AdvanceTimeSecs(double seconds) { now_ += seconds; }
  void AdvanceTimeMillisecs(double ms) { now_ += (ms / kNumMillisecsPerSec); }

 private:
  double now_;
};

}  // namespace rtc

#endif  // WEBRTC_BASE_TEST_FAKETIMING_H_
