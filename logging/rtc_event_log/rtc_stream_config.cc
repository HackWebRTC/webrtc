/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/rtc_stream_config.h"

namespace webrtc {
namespace rtclog {

StreamConfig::StreamConfig() {}

StreamConfig::~StreamConfig() {}

StreamConfig::Codec::Codec(const std::string& payload_name,
                           int payload_type,
                           int rtx_payload_type)
        : payload_name(payload_name),
          payload_type(payload_type),
          rtx_payload_type(rtx_payload_type) {}


}  // namespace rtclog
}  // namespace webrtc
