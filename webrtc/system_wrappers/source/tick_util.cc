/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "system_wrappers/interface/tick_util.h"

#include <cassert>

namespace webrtc {

bool TickTime::_use_fake_clock = false;
WebRtc_Word64 TickTime::_fake_ticks = 0;

void TickTime::UseFakeClock(WebRtc_Word64 start_millisecond) {
  _use_fake_clock = true;
  _fake_ticks = MillisecondsToTicks(start_millisecond);
}

void TickTime::AdvanceFakeClock(WebRtc_Word64 milliseconds) {
  assert(_use_fake_clock);
  _fake_ticks += MillisecondsToTicks(milliseconds);
}

}  // namespace webrtc
