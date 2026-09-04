/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef API_ROQ_ROQ_SESSION_H_
#define API_ROQ_ROQ_SESSION_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "system_wrappers/include/clock.h"

namespace webrtc::roq {

enum class RoqSendResult { kQueued, kQueueFull, kOversize, kInvalid, kClosed };
enum class RoqMediaType { kAudio, kVideo, kRtcp };
enum class RoqDatagramPriority {
  kPeriodicRtcp,
  kVideoDelta,
  kVideoKey,
  kAudio,
  kRtcp,
};
enum class RoqReceiveFlowPolicy { kDeliver, kDiscard };

struct RoqFlowConfig {
  uint64_t flow_id = 0;
  RoqReceiveFlowPolicy receive_policy = RoqReceiveFlowPolicy::kDeliver;
  bool allow_send = false;
};

struct RoqFrameId {
  uint32_t ssrc = 0;
  uint32_t timestamp = 0;

  bool operator==(const RoqFrameId&) const = default;
};

struct RoqPacketId {
  uint32_t ssrc = 0;
  uint16_t sequence_number = 0;

  bool operator==(const RoqPacketId&) const = default;
};

struct RoqDatagramMetadata {
  RoqMediaType media_type = RoqMediaType::kVideo;
  Timestamp enqueue_time = Timestamp::MinusInfinity();
  Timestamp deadline = Timestamp::PlusInfinity();
  RoqDatagramPriority priority = RoqDatagramPriority::kVideoDelta;
  std::optional<RoqFrameId> frame_id;
  std::optional<RoqPacketId> packet_id;
  std::optional<RoqPacketId> original_packet_id;
};

struct RoqSessionConfig {
  std::string address;
  std::string server_name = "quic.tech";
  std::string alpn = "roq-14";
  std::string trusted_ca_file;
  std::string trusted_ca_directory;
  std::string resumption_cache_directory;
  std::array<uint8_t, 32> resumption_context = {};
  TimeDelta resumption_cache_max_age = TimeDelta::Seconds(24 * 60 * 60);
  std::function<Timestamp()> resumption_clock;
  size_t maximum_udp_payload = 1350;
  uint64_t max_idle_timeout_ms = 5000;
  size_t outgoing_queue_capacity = 512;
  Clock* clock = Clock::GetRealTimeClock();
  std::vector<uint64_t> allowed_flow_ids;
  std::vector<uint64_t> receive_flow_ids;
};

struct RoqReceivedPacket {
  uint64_t flow_id = 0;
  std::vector<uint8_t> packet;
};

struct RoqSessionStats {
  bool established = false;
  bool clean_shutdown = false;
  std::vector<uint8_t> source_connection_id;
  bool resumption_cache_hit = false;
  bool resumption_cache_expired = false;
  bool resumption_cache_invalid = false;
  bool resumption_cache_saved = false;
  bool session_resumed = false;
  bool early_data_attempted = false;
  bool early_data_accepted = false;
  bool early_data_fallback = false;
  uint32_t early_data_reason = 0;
  size_t reliable_messages_sent = 0;
  size_t reliable_messages_received = 0;
  size_t early_reliable_messages_received = 0;
  size_t address_tokens_received = 0;
  size_t early_datagrams_received = 0;
  size_t early_datagrams_sent = 0;
  size_t early_rejection_queue_drops = 0;
  size_t unknown_flow_buffered = 0;
  size_t unknown_flow_released = 0;
  size_t unknown_flow_dropped = 0;
  size_t sent_datagrams = 0;
  size_t received_datagrams = 0;
  size_t sent_udp_packets = 0;
  size_t received_udp_packets = 0;
  size_t queue_full_drops = 0;
  size_t oversize_drops = 0;
  size_t pmtu_purge_drops = 0;
  size_t expired_drops = 0;
  size_t invalid_drops = 0;
  size_t quiche_slot_expired_drops = 0;
  size_t peak_queue_size = 0;
  int64_t max_queue_delay_us = 0;
  size_t dropped_periodic_rtcp = 0;
  size_t dropped_video_delta = 0;
  size_t dropped_video_key = 0;
  size_t dropped_audio = 0;
  size_t dropped_critical_rtcp = 0;
  size_t max_datagram_payload = 0;
  uint64_t max_pacing_delay_ns = 0;
  uint64_t rtt_ns = 0;
  uint64_t max_rtt_ns = 0;
  uint64_t delivery_rate_bytes_per_second = 0;
  uint64_t max_bandwidth_bytes_per_second = 0;
  uint64_t acked_bytes = 0;
  uint64_t startup_exit_cwnd = 0;
  size_t pmtu_bytes = 0;
  size_t dgram_lost = 0;
};

class RoqSession {
 public:
  using ReceiveCallback = std::function<void(RoqReceivedPacket)>;
  using DatagramSizeCallback = std::function<void(size_t)>;
  using ReliableMessageCallback = std::function<void(std::vector<uint8_t>)>;

  static std::unique_ptr<RoqSession> Create(
      RoqSessionConfig config,
      ReceiveCallback receive_callback,
      DatagramSizeCallback datagram_size_callback = {},
      ReliableMessageCallback reliable_message_callback = {});

  virtual ~RoqSession() = default;
  virtual bool Start(std::string* error) = 0;
  virtual bool WaitUntilReady(TimeDelta timeout) = 0;
  virtual bool WaitUntilEstablished(TimeDelta timeout) = 0;
  virtual bool SendReplaySafeMessage(std::span<const uint8_t> message) = 0;
  virtual bool WaitUntilReliableMessagesSent(size_t count, TimeDelta timeout) {
    (void)count;
    (void)timeout;
    return true;
  }
  virtual bool WaitUntilReliableMessagesDrained(TimeDelta timeout) {
    (void)timeout;
    return true;
  }
  virtual void BeginReceiveFlowUpdate() = 0;
  virtual void UpdateReceiveFlowIds(std::vector<uint64_t> flow_ids) = 0;
  virtual void BeginFlowUpdate() { BeginReceiveFlowUpdate(); }
  virtual void CommitFlowUpdate(std::vector<RoqFlowConfig> flows) {
    std::vector<uint64_t> receive_flow_ids;
    receive_flow_ids.reserve(flows.size());
    for (const RoqFlowConfig& flow : flows) {
      if (flow.receive_policy == RoqReceiveFlowPolicy::kDeliver)
        receive_flow_ids.push_back(flow.flow_id);
    }
    UpdateReceiveFlowIds(std::move(receive_flow_ids));
  }
  virtual RoqSendResult SendMedia(uint64_t flow_id,
                                  std::span<const uint8_t> packet,
                                  RoqDatagramMetadata metadata,
                                  std::function<void()> on_submitted) = 0;
  virtual void RecordInvalidDatagram() = 0;
  virtual void UpdatePeerDatagramPayloadLimit(size_t bytes) = 0;
  virtual void Close(uint64_t application_error = 0,
                     std::string reason = "complete") = 0;
  virtual bool WaitUntilClosed(TimeDelta timeout) = 0;
  virtual RoqSessionStats stats() const = 0;
  virtual std::string error() const = 0;
};

}  // namespace webrtc::roq

#endif  // API_ROQ_ROQ_SESSION_H_
