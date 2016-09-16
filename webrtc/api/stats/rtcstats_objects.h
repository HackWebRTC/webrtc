/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_STATS_RTCSTATS_OBJECTS_H_
#define WEBRTC_API_STATS_RTCSTATS_OBJECTS_H_

#include <string>

#include "webrtc/api/stats/rtcstats.h"

namespace webrtc {

class RTCPeerConnectionStats : public RTCStats {
 public:
  RTCPeerConnectionStats(const std::string& id, int64_t timestamp_us);
  RTCPeerConnectionStats(std::string&& id, int64_t timestamp_us);

  WEBRTC_RTCSTATS_IMPL(RTCStats, RTCPeerConnectionStats,
      &data_channels_opened,
      &data_channels_closed);

  RTCStatsMember<uint32_t> data_channels_opened;
  RTCStatsMember<uint32_t> data_channels_closed;
};

}  // namespace webrtc

#endif  // WEBRTC_API_STATS_RTCSTATS_OBJECTS_H_
