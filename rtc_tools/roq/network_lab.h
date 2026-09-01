/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_ROQ_NETWORK_LAB_H_
#define RTC_TOOLS_ROQ_NETWORK_LAB_H_

#include <cstddef>
#include <string>

namespace webrtc::roq {

struct UdpSelfTestResult {
  size_t sent = 0;
  size_t received = 0;
  size_t forwarded = 0;
  size_t injected_loss = 0;
  size_t injected_reorder = 0;
  double average_rtt_ms = 0;
  double min_rtt_ms = 0;
  double max_rtt_ms = 0;
  std::string error;
};

UdpSelfTestResult RunUdpEchoSelfTest(size_t packet_count);
UdpSelfTestResult RunUdpRelaySelfTest(size_t packet_count,
                                      const std::string& run_id,
                                      const std::string& event_log_path);
UdpSelfTestResult RunUdpRelay(const std::string& listen_address,
                              const std::string& forward_address,
                              double duration_seconds,
                              const std::string& run_id,
                              const std::string& event_log_path);

}  // namespace webrtc::roq

#endif  // RTC_TOOLS_ROQ_NETWORK_LAB_H_
