/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef RTC_TOOLS_ROQ_RESUMPTION_PEER_H_
#define RTC_TOOLS_ROQ_RESUMPTION_PEER_H_

#include <cstdint>
#include <string>

#include "rtc_tools/roq/roq_peer.h"

namespace webrtc::roq {

struct ResumptionPeerOptions {
  PeerRole role = PeerRole::kClient;
  std::string address;
  std::string executable_path;
  std::string cache_directory;
  uint8_t context_byte = 1;
  int64_t cache_max_age_ms = 24 * 60 * 60 * 1000;
};

struct ResumptionPeerResult {
  bool established = false;
  bool cache_hit = false;
  bool cache_saved = false;
  bool cache_expired = false;
  bool cache_invalid = false;
  bool resumed = false;
  bool early_attempted = false;
  bool early_accepted = false;
  bool fallback = false;
  uint32_t early_reason = 0;
  size_t reliable_messages_sent = 0;
  size_t reliable_messages_received = 0;
  size_t early_reliable_messages_received = 0;
  size_t address_tokens_received = 0;
  size_t address_tokens_sent = 0;
  bool peer_address_validated = false;
  std::string error;
};

ResumptionPeerResult RunResumptionPeer(const ResumptionPeerOptions& options);

}  // namespace webrtc::roq

#endif  // RTC_TOOLS_ROQ_RESUMPTION_PEER_H_
