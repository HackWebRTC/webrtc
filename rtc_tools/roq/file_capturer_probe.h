/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef RTC_TOOLS_ROQ_FILE_CAPTURER_PROBE_H_
#define RTC_TOOLS_ROQ_FILE_CAPTURER_PROBE_H_

#include <cstdint>
#include <string>

namespace webrtc::roq {

struct FileCapturerProbeResult {
  int video_frames = 0;
  int64_t audio_samples_per_channel = 0;
  int64_t audio_blocks = 0;
  int audio_sample_rate_hz = 0;
  int audio_channels = 0;
  int video_width = 0;
  int video_height = 0;
  uint64_t video_hash = 0;
  uint64_t audio_hash = 0;
  uint64_t video_pts_hash = 0;
  uint64_t audio_pts_hash = 0;
  int64_t max_pacing_error_us = 0;
  std::string error;
};

FileCapturerProbeResult RunFileCapturerProbe(const std::string& input_path,
                                             double duration_seconds,
                                             const std::string& run_id,
                                             const std::string& event_log_path);

std::string HexHash(uint64_t value);

}  // namespace webrtc::roq

#endif  // RTC_TOOLS_ROQ_FILE_CAPTURER_PROBE_H_
