/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_ROQ_QUICHE_ABI_H_
#define RTC_TOOLS_ROQ_QUICHE_ABI_H_

#include <cstddef>
#include <cstdint>
#include <string>

namespace webrtc::roq {

struct QuicheAbiResult {
  uint64_t max_pacing_delay_ns = 0;
  uint64_t rtt_ns = 0;
  uint64_t min_rtt_ns = 0;
  uint64_t max_rtt_ns = 0;
  uint64_t delivery_rate_bytes_per_second = 0;
  uint64_t max_bandwidth_bytes_per_second = 0;
  size_t pmtu_bytes = 0;
  size_t dgram_lost = 0;
  uint32_t early_data_accepted_reason = 0;
  uint32_t early_data_rejected_reason = 0;
  bool rejection_fell_back_to_1rtt = false;
  std::string error;
};

QuicheAbiResult RunQuicheAbiChecks(const std::string& executable_path);

}  // namespace webrtc::roq

#endif  // RTC_TOOLS_ROQ_QUICHE_ABI_H_
