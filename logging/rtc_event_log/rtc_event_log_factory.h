/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_FACTORY_H_
#define LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_FACTORY_H_

#include <memory>

#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/rtc_event_log/rtc_event_log_factory_interface.h"

namespace webrtc {

// TODO(bugs.webrtc.org/10284): Stop using the RtcEventLogFactory factory.
std::unique_ptr<RtcEventLogFactoryInterface> CreateRtcEventLogFactory();
}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_FACTORY_H_
