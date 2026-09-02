/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_ROQ_MEDIA_SOURCE_H_
#define RTC_TOOLS_ROQ_MEDIA_SOURCE_H_

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>

namespace webrtc::roq {

enum class MediaClockMode {
  kVirtual,
  kRealtime,
};

struct MediaSourceResult {
  int video_frames = 0;
  int64_t audio_samples_per_channel = 0;
  int64_t audio_blocks = 0;
  int audio_block_fill = 0;
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

struct MediaVideoFrame {
  int width = 0;
  int height = 0;
  const uint8_t* data_y = nullptr;
  const uint8_t* data_u = nullptr;
  const uint8_t* data_v = nullptr;
  int stride_y = 0;
  int stride_u = 0;
  int stride_v = 0;
  int64_t pts_us = 0;
};

struct MediaSourceCallbacks {
  std::function<void(const MediaVideoFrame&)> on_video_frame;
  std::function<void(std::span<const int16_t>, int64_t)> on_audio_block;
  std::function<void(size_t)> on_loop_started;
  std::function<bool()> should_stop;
};

MediaSourceResult RunMediaSource(const std::string& input_path,
                                 double duration_seconds,
                                 MediaClockMode clock_mode,
                                 const std::string& run_id = "",
                                 const std::string& event_log_path = "");

MediaSourceResult StreamMediaSource(const std::string& input_path,
                                    std::optional<double> duration_seconds,
                                    bool loop_at_eof,
                                    MediaClockMode clock_mode,
                                    const MediaSourceCallbacks& callbacks,
                                    const std::string& run_id = "",
                                    const std::string& event_log_path = "");

std::string HexHash(uint64_t value);

}  // namespace webrtc::roq

#endif  // RTC_TOOLS_ROQ_MEDIA_SOURCE_H_
