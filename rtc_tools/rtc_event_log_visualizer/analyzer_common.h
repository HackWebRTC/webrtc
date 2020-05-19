/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_RTC_EVENT_LOG_VISUALIZER_ANALYZER_COMMON_H_
#define RTC_TOOLS_RTC_EVENT_LOG_VISUALIZER_ANALYZER_COMMON_H_

#include <cstdint>
#include <string>

#include "logging/rtc_event_log/rtc_event_log_parser.h"

namespace webrtc {

class AnalyzerConfig {
 public:
  float GetCallTimeSec(int64_t timestamp_us) const {
    int64_t offset = normalize_time_ ? begin_time_ : 0;
    return static_cast<float>(timestamp_us - offset) / 1000000;
  }

  float CallBeginTimeSec() const { return GetCallTimeSec(begin_time_); }

  float CallEndTimeSec() const { return GetCallTimeSec(end_time_); }

  // Window and step size used for calculating moving averages, e.g. bitrate.
  // The generated data points will be |step_| microseconds apart.
  // Only events occurring at most |window_duration_| microseconds before the
  // current data point will be part of the average.
  int64_t window_duration_;
  int64_t step_;

  // First and last events of the log.
  int64_t begin_time_;
  int64_t end_time_;
  bool normalize_time_;
};

struct LayerDescription {
  LayerDescription(uint32_t ssrc, uint8_t spatial_layer, uint8_t temporal_layer)
      : ssrc(ssrc),
        spatial_layer(spatial_layer),
        temporal_layer(temporal_layer) {}
  bool operator<(const LayerDescription& other) const {
    if (ssrc != other.ssrc)
      return ssrc < other.ssrc;
    if (spatial_layer != other.spatial_layer)
      return spatial_layer < other.spatial_layer;
    return temporal_layer < other.temporal_layer;
  }
  uint32_t ssrc;
  uint8_t spatial_layer;
  uint8_t temporal_layer;
};

bool IsRtxSsrc(const ParsedRtcEventLog& parsed_log,
               PacketDirection direction,
               uint32_t ssrc);
bool IsVideoSsrc(const ParsedRtcEventLog& parsed_log,
                 PacketDirection direction,
                 uint32_t ssrc);
bool IsAudioSsrc(const ParsedRtcEventLog& parsed_log,
                 PacketDirection direction,
                 uint32_t ssrc);

std::string GetStreamName(const ParsedRtcEventLog& parsed_log,
                          PacketDirection direction,
                          uint32_t ssrc);
std::string GetLayerName(LayerDescription layer);

}  // namespace webrtc

#endif  // RTC_TOOLS_RTC_EVENT_LOG_VISUALIZER_ANALYZER_COMMON_H_
