/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_ROQ_ROQ_PEER_H_
#define RTC_TOOLS_ROQ_ROQ_PEER_H_

#include <cstddef>
#include <cstdint>
#include <string>

namespace webrtc::roq {

enum class PeerRole { kClient, kServer };

struct SyntheticRoqOptions {
  PeerRole role = PeerRole::kClient;
  std::string address;
  std::string manifest_path;
  std::string executable_path;
  std::string event_log_path;
  std::string run_id;
  std::string inject_error;
  double duration_seconds = 5;
  bool disable_datagram = false;
};

struct SyntheticRoqResult {
  std::string alpn;
  bool datagram_negotiated = false;
  bool peer_verification = false;
  size_t generated_audio_rtp = 0;
  size_t generated_video_rtp = 0;
  size_t generated_rtcp = 0;
  size_t received_audio_rtp = 0;
  size_t received_video_rtp = 0;
  size_t received_rtcp = 0;
  size_t queue_full_drop = 0;
  size_t malformed = 0;
  size_t unknown_flow = 0;
  size_t duplicate = 0;
  size_t reordered = 0;
  bool audio_received_in_final_second = false;
  bool video_received_in_final_second = false;
  uint64_t close_code = 0;
  bool close_is_application = true;
  bool close_observed = false;
  uint64_t max_pacing_delay_ns = 0;
  uint64_t rtt_ns = 0;
  uint64_t max_rtt_ns = 0;
  uint64_t delivery_rate_bytes_per_second = 0;
  uint64_t max_bandwidth_bytes_per_second = 0;
  size_t pmtu_bytes = 0;
  size_t dgram_lost = 0;
  std::string error;
};

SyntheticRoqResult RunSyntheticRoq(const SyntheticRoqOptions& options);

}  // namespace webrtc::roq

#endif  // RTC_TOOLS_ROQ_ROQ_PEER_H_
