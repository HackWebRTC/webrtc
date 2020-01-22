/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/test/time_controller.h"

namespace webrtc {
bool TimeController::Wait(const std::function<bool()>& done,
                          TimeDelta max_duration) {
  // Step size is chosen to be short enough to not significantly affect latency
  // in real time tests while being long enough to avoid adding too much load to
  // the system.
  const auto kStep = TimeDelta::ms(5);
  for (auto elapsed = TimeDelta::Zero(); elapsed < max_duration;
       elapsed += kStep) {
    if (done())
      return true;
    AdvanceTime(kStep);
  }
  return done();
}
}  // namespace webrtc
