/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "rtc_tools/roq/resumption_peer.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "api/roq/roq_session.h"
#include "api/units/time_delta.h"
#include "rtc_tools/roq/test_roq_server.h"

namespace webrtc::roq {
namespace {

constexpr std::array<uint8_t, 16> kTestAddressToken = {
    0x72, 0x6f, 0x71, 0x2d, 0x6d, 0x34, 0x2d, 0x74,
    0x6f, 0x6b, 0x65, 0x6e, 0x2d, 0x30, 0x30, 0x31,
};
constexpr std::array<uint8_t, 9> kRequest = {
    'r', 'e', 's', 'u', 'm', 'e', '-', 'v', '1',
};

ResumptionPeerResult FromStats(const RoqSessionStats& stats) {
  return {
      .established = stats.established,
      .cache_hit = stats.resumption_cache_hit,
      .cache_saved = stats.resumption_cache_saved,
      .cache_expired = stats.resumption_cache_expired,
      .cache_invalid = stats.resumption_cache_invalid,
      .resumed = stats.session_resumed,
      .early_attempted = stats.early_data_attempted,
      .early_accepted = stats.early_data_accepted,
      .fallback = stats.early_data_fallback,
      .early_reason = stats.early_data_reason,
      .reliable_messages_sent = stats.reliable_messages_sent,
      .reliable_messages_received = stats.reliable_messages_received,
      .early_reliable_messages_received =
          stats.early_reliable_messages_received,
      .address_tokens_received = stats.address_tokens_received,
  };
}

}  // namespace

ResumptionPeerResult RunResumptionPeer(const ResumptionPeerOptions& options) {
  std::array<uint8_t, 32> context = {};
  context.fill(options.context_byte);
  const std::filesystem::path executable =
      std::filesystem::absolute(options.executable_path);
  std::atomic<size_t> received_messages = 0;
  std::unique_ptr<RoqSession> session;
  TestRoqServer* test_server = nullptr;
  if (options.role == PeerRole::kClient) {
    session = RoqSession::Create(
        {.address = options.address,
         .trusted_ca_file = (executable.parent_path() / "cert.crt").string(),
         .resumption_cache_directory = options.cache_directory,
         .resumption_context = context,
         .resumption_cache_max_age =
             TimeDelta::Millis(options.cache_max_age_ms),
         .allowed_flow_ids = {0, 1, 2}},
        [](RoqReceivedPacket) {}, {},
        [&received_messages](std::vector<uint8_t>) { ++received_messages; });
    if (!session || !session->SendReplaySafeMessage(kRequest))
      return {.error = "failed to queue replay-safe request"};
  } else {
    std::unique_ptr<TestRoqServer> server = CreateTestRoqServer(
        {.address = options.address,
         .certificate_path = (executable.parent_path() / "cert.crt").string(),
         .private_key_path = (executable.parent_path() / "cert.key").string(),
         .resumption_context = context,
         .address_token = std::vector<uint8_t>(kTestAddressToken.begin(),
                                               kTestAddressToken.end()),
         .allowed_flow_ids = {0, 1, 2}},
        [](RoqReceivedPacket) {}, {},
        [&received_messages](std::vector<uint8_t> message) {
          if (message == std::vector<uint8_t>(kRequest.begin(), kRequest.end()))
            ++received_messages;
        });
    test_server = server.get();
    session = std::move(server);
  }
  std::string error;
  if (!session || !session->Start(&error) ||
      !session->WaitUntilReady(TimeDelta::Seconds(3))) {
    return {.error = error.empty() ? session->error() : error};
  }
  if (options.role == PeerRole::kServer)
    std::cout << "{\"schema_version\":1,\"event\":\"ready\"}\n" << std::flush;
  if (!session->WaitUntilEstablished(TimeDelta::Seconds(10))) {
    const RoqSessionStats failed = session->stats();
    return {.error = session->error().empty()
                         ? "handshake timed out: sent_udp=" +
                               std::to_string(failed.sent_udp_packets) +
                               " received_udp=" +
                               std::to_string(failed.received_udp_packets)
                         : session->error()};
  }
  if (options.role == PeerRole::kClient) {
    const auto established_at = std::chrono::steady_clock::now();
    const auto deadline = established_at + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
      const RoqSessionStats stats = session->stats();
      if (stats.reliable_messages_sent > 0 &&
          (options.cache_directory.empty() ||
           (stats.resumption_cache_saved &&
            stats.address_tokens_received > 0)) &&
          std::chrono::steady_clock::now() - established_at >=
              std::chrono::milliseconds(300)) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    session->Close();
    session->WaitUntilClosed(TimeDelta::Seconds(2));
  } else if (!session->WaitUntilClosed(TimeDelta::Seconds(3))) {
    session->Close();
    session->WaitUntilClosed(TimeDelta::Seconds(2));
  }
  ResumptionPeerResult result = FromStats(session->stats());
  if (test_server) {
    const TestRoqServerStats stats = test_server->test_stats();
    result.address_tokens_sent = stats.address_tokens_sent;
    result.peer_address_validated = stats.peer_address_validated;
  }
  result.error = session->error();
  return result;
}

}  // namespace webrtc::roq
