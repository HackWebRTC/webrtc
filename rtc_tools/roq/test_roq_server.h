/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef RTC_TOOLS_ROQ_TEST_ROQ_SERVER_H_
#define RTC_TOOLS_ROQ_TEST_ROQ_SERVER_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "api/roq/roq_session.h"
#include "system_wrappers/include/clock.h"

namespace webrtc::roq {

struct TestRoqServerConfig {
  std::string address;
  std::string alpn = "roq-14";
  std::string certificate_path;
  std::string private_key_path;
  std::array<uint8_t, 32> resumption_context = {};
  std::vector<uint8_t> address_token;
  bool gate_media_until_reliable_message = false;
  bool replay_first_provisional = false;
  bool drop_all_provisional = false;
  size_t maximum_udp_payload = 1350;
  size_t outgoing_queue_capacity = 512;
  Clock* clock = Clock::GetRealTimeClock();
  std::vector<uint64_t> allowed_flow_ids;
};

struct TestRoqServerStats {
  size_t address_tokens_sent = 0;
  bool peer_address_validated = false;
  size_t zero_rtt_packets_received = 0;
  size_t provisional_datagrams_received = 0;
  size_t provisional_datagrams_committed = 0;
  size_t provisional_datagrams_dropped = 0;
  size_t provisional_datagrams_deduplicated = 0;
};

class TestRoqServer : public RoqSession {
 public:
  virtual TestRoqServerStats test_stats() const = 0;
};

std::unique_ptr<TestRoqServer> CreateTestRoqServer(
    TestRoqServerConfig config,
    RoqSession::ReceiveCallback receive_callback,
    RoqSession::DatagramSizeCallback datagram_size_callback = {},
    RoqSession::ReliableMessageCallback reliable_message_callback = {});

}  // namespace webrtc::roq

#endif  // RTC_TOOLS_ROQ_TEST_ROQ_SERVER_H_
