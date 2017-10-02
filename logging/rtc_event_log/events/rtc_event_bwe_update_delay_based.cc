/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_bwe_update_delay_based.h"

namespace webrtc {

RtcEvent::Type RtcEventBweUpdateDelayBased::GetType() const {
  return RtcEvent::Type::BweUpdateDelayBased;
}

bool RtcEventBweUpdateDelayBased::IsConfigEvent() const {
  return false;
}

}  // namespace webrtc
