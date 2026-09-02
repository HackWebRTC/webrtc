/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef API_ROQ_ROQ_SESSION_H_
#define API_ROQ_ROQ_SESSION_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "api/units/time_delta.h"

namespace webrtc::roq {

enum class RoqRole { kClient, kServer };
enum class RoqSendResult { kQueued, kQueueFull, kOversize, kClosed };

struct RoqSessionConfig {
  RoqRole role = RoqRole::kClient;
  std::string address;
  std::string server_name = "quic.tech";
  std::string alpn = "roq-14";
  std::string certificate_path;
  std::string private_key_path;
  std::string trusted_ca_file;
  std::string trusted_ca_directory;
  bool verify_peer = true;
  size_t maximum_udp_payload = 1350;
  size_t outgoing_queue_capacity = 512;
  std::vector<uint64_t> allowed_flow_ids;
};

struct RoqReceivedPacket {
  uint64_t flow_id = 0;
  std::vector<uint8_t> packet;
};

struct RoqSessionStats {
  bool established = false;
  bool clean_shutdown = false;
  size_t sent_datagrams = 0;
  size_t received_datagrams = 0;
  size_t sent_udp_packets = 0;
  size_t received_udp_packets = 0;
  size_t queue_full_drops = 0;
  size_t oversize_drops = 0;
  size_t pmtu_purge_drops = 0;
  size_t peak_queue_size = 0;
  size_t max_datagram_payload = 0;
  uint64_t max_pacing_delay_ns = 0;
  uint64_t rtt_ns = 0;
  uint64_t max_rtt_ns = 0;
  uint64_t delivery_rate_bytes_per_second = 0;
  uint64_t max_bandwidth_bytes_per_second = 0;
  size_t pmtu_bytes = 0;
  size_t dgram_lost = 0;
};

class RoqSession {
 public:
  using ReceiveCallback = std::function<void(RoqReceivedPacket)>;
  using DatagramSizeCallback = std::function<void(size_t)>;

  static std::unique_ptr<RoqSession> Create(
      RoqSessionConfig config,
      ReceiveCallback receive_callback,
      DatagramSizeCallback datagram_size_callback = {});

  virtual ~RoqSession() = default;
  virtual bool Start(std::string* error) = 0;
  virtual bool WaitUntilReady(TimeDelta timeout) = 0;
  virtual bool WaitUntilEstablished(TimeDelta timeout) = 0;
  virtual RoqSendResult Send(uint64_t flow_id,
                             std::span<const uint8_t> packet) = 0;
  virtual void SetApplicationDatagramPayloadLimit(size_t bytes) = 0;
  virtual void Close(uint64_t application_error = 0,
                     std::string reason = "complete") = 0;
  virtual bool WaitUntilClosed(TimeDelta timeout) = 0;
  virtual RoqSessionStats stats() const = 0;
  virtual std::string error() const = 0;
};

}  // namespace webrtc::roq

#endif  // API_ROQ_ROQ_SESSION_H_
