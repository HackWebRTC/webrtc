/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef API_ROQ_ROQ_SESSION_H_
#define API_ROQ_ROQ_SESSION_H_

#include <array>
#include <atomic>
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
#include "api/video/video_codec_type.h"
#include "system_wrappers/include/clock.h"

namespace webrtc::roq {

// DATAGRAM payload, including the RoQ flow varint (not an RTP limit).
inline constexpr size_t kRoqEarlyDatagramPayloadLimit = 1000;
inline size_t RoqPacketPayloadLimit(size_t datagram_bytes, uint64_t flow_id) {
  const size_t prefix = flow_id < (1ULL << 6)    ? 1
                        : flow_id < (1ULL << 14) ? 2
                        : flow_id < (1ULL << 30) ? 4
                                                 : 8;
  return flow_id < (1ULL << 62) && datagram_bytes > prefix
             ? datagram_bytes - prefix
             : 0;
}

enum class RoqSendResult {
  kQueued,
  kDropped,
  kQueueFull,
  kOversize,
  kInvalid,
  kClosed
};
enum class RoqMediaType { kAudio, kVideo, kRtcp };
enum class RoqUdpSendDisposition { kSent, kRetry, kFatal };
RoqUdpSendDisposition ClassifyUdpSendResult(int64_t result,
                                            size_t expected_bytes,
                                            int error);
bool ShouldContinueDatagramBurst(size_t attempted,
                                 size_t limit,
                                 bool pending_pacing,
                                 size_t quiche_queue_length,
                                 size_t pending_media);
enum class RoqDatagramPriority {
  kPeriodicRtcp,
  kVideoDelta,
  kVideoKey,
  kRetransmission,
  kAudio,
  kRtcp,
};
enum class RoqReceiveFlowPolicy { kDeliver, kDiscard };
enum class RoqSessionLifecycleState { kReady, kEstablished, kClosed };

struct RoqSessionLifecycleEvent {
  RoqSessionLifecycleState state = RoqSessionLifecycleState::kReady;
  std::string error;
};

struct RoqFlowConfig {
  uint64_t flow_id = 0;
  RoqReceiveFlowPolicy receive_policy = RoqReceiveFlowPolicy::kDeliver;
  bool allow_send = false;
  uint64_t binding_generation = 0;
};

struct RoqFrameId {
  uint32_t ssrc = 0;
  uint32_t timestamp = 0;
  std::string binding_id;

  bool operator==(const RoqFrameId&) const = default;
};

struct RoqPacketId {
  uint32_t ssrc = 0;
  uint16_t sequence_number = 0;

  bool operator==(const RoqPacketId&) const = default;
};

// Shared by the external queue, quiche slot and immutable UDP payload. The
// last owner dropping it is a cancellation; completing a copy is idempotent.
// Callbacks must be non-blocking and must not re-enter a session.
class RoqDatagramCompletion {
 public:
  explicit RoqDatagramCompletion(std::function<void(bool, int64_t)> callback)
      : callback_(std::move(callback)) {}
  ~RoqDatagramCompletion() { Complete(false); }
  void Complete(bool sent, int64_t at_us = -1) {
    if (!completed_.exchange(true) && callback_)
      callback_(sent, at_us);
  }

 private:
  std::atomic<bool> completed_ = false;
  const std::function<void(bool, int64_t)> callback_;
};

struct RoqDatagramMetadata {
  std::string binding_id;
  RoqMediaType media_type = RoqMediaType::kVideo;
  Timestamp enqueue_time = Timestamp::MinusInfinity();
  Timestamp deadline = Timestamp::PlusInfinity();
  RoqDatagramPriority priority = RoqDatagramPriority::kVideoDelta;
  std::optional<RoqFrameId> frame_id;
  std::optional<RoqPacketId> packet_id;
  std::optional<RoqPacketId> original_packet_id;
  bool frame_complete = false;
  VideoCodecType video_codec = VideoCodecType::kVideoCodecGeneric;
  bool is_key = false;
  bool contains_vps = false;
  bool contains_sps = false;
  bool contains_pps = false;
  uint32_t rtp_timestamp = 0;
  size_t primary_payload_bytes = 0;
  size_t redundant_payload_bytes = 0;
  size_t redundant_blocks = 0;
  std::vector<uint8_t> periodic_rtcp_key;
  std::shared_ptr<RoqDatagramCompletion> completion;
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
  bool allow_early_datagrams = false;
  // Buffer new receive flows for at most 100 ms while unsolicited control
  // updates install their mappings. Disabled for static-flow applications.
  bool buffer_dynamic_receive_flows = false;
  size_t maximum_udp_payload = 1350;
  uint64_t max_idle_timeout_ms = 5000;
  size_t outgoing_queue_capacity = 512;
  size_t outgoing_queue_byte_capacity = 512 * 1024;
  Clock* clock = Clock::GetRealTimeClock();
  std::function<void(RoqSessionLifecycleEvent)> lifecycle_callback;
  std::vector<uint64_t> allowed_flow_ids;
  std::vector<uint64_t> receive_flow_ids;
};

struct RoqReceivedPacket {
  uint64_t flow_id = 0;
  std::vector<uint8_t> packet;
  int64_t received_at_us = -1;
  int64_t media_enqueued_at_us = -1;
  uint64_t transport_generation = 0;
  uint64_t binding_generation = 0;
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
  uint64_t expired_video_frames = 0;
  uint64_t oldest_video_queue_age_us = 0;
  uint64_t max_video_queue_age_us = 0;
  size_t invalid_drops = 0;
  size_t quiche_slot_expired_drops = 0;
  size_t peak_quiche_queue_size = 0;
  size_t peak_queue_size = 0;
  size_t peak_queue_bytes = 0;
  size_t queued_packets = 0;
  size_t queued_bytes = 0;
  int64_t max_queue_delay_us = 0;
  size_t dropped_periodic_rtcp = 0;
  size_t dropped_video_delta = 0;
  size_t dropped_video_key = 0;
  size_t dropped_audio = 0;
  size_t dropped_critical_rtcp = 0;
  size_t dependency_drops = 0;
  size_t recovery_requests = 0;
  size_t network_control_updates = 0;
  size_t controller_probe_cluster_count = 0;
  int64_t controller_target_rate_bps = 0;
  int64_t controller_pacer_rate_bps = 0;
  int64_t controller_padding_rate_bps = 0;
  bool controller_has_congestion_window = false;
  size_t max_datagram_payload = 0;
  uint64_t max_pacing_delay_ns = 0;
  uint64_t rtt_ns = 0;
  uint64_t max_rtt_ns = 0;
  uint64_t delivery_rate_bytes_per_second = 0;
  uint64_t max_bandwidth_bytes_per_second = 0;
  uint64_t acked_bytes = 0;
  uint64_t startup_exit_cwnd = 0;
  uint64_t current_cwnd_bytes = 0;
  uint64_t lost_packets = 0;
  uint64_t lost_bytes = 0;
  size_t pmtu_bytes = 0;
  size_t dgram_lost = 0;
  // std::chrono::steady_clock epoch, shared with session liveness checks.
  int64_t last_ack_monotonic_us = -1;
  int64_t configured_audio_bitrate_bps = 0;
  int64_t configured_max_media_bitrate_bps = 0;
  int64_t configured_resume_media_bitrate_bps = 0;
  uint64_t bandwidth_evidence_round = 0;
  uint64_t bandwidth_evidence_bytes_per_second = 0;
  uint64_t bandwidth_evidence_age_ns = UINT64_MAX;
  uint64_t maintenance_wire_bytes = 0;
  uint64_t audio_payload_bytes = 0;
  uint64_t audio_wire_bytes = 0;
  uint64_t audio_packets = 0;
  uint64_t audio_redundant_payload_bytes = 0;
  uint64_t audio_redundant_blocks = 0;
  uint64_t configured_audio_packet_rate = 50;
  uint64_t video_payload_bytes = 0;
  uint64_t video_wire_bytes = 0;
  uint64_t rtcp_wire_bytes = 0;
  uint64_t recovery_wire_bytes = 0;
  uint64_t probe_wire_bytes = 0;
  uint64_t sent_wire_bytes = 0;
  size_t recovery_budget_drops = 0;
  size_t coalesced_reports = 0;
};

struct RoqBindingSendStats {
  size_t enqueued_packets = 0;
  size_t queued_packets = 0;
  size_t queued_bytes = 0;
  size_t submitted_packets = 0;
  size_t dropped_packets = 0;
  size_t dependency_drops = 0;
  size_t recovery_requests = 0;
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
  virtual void SetEarlyDatagramsAllowed(bool allowed) = 0;
  virtual void RequestPing() = 0;
  virtual void Abort() = 0;
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
  // Control has validated these flows, but their media consumers are not ready.
  virtual void PrepareReceiveFlowIds(std::vector<uint64_t>) {}
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
  virtual void SetMediaWireBudget(uint64_t probe_max_bytes_per_second,
                                  uint64_t recovery_bytes_per_second,
                                  uint64_t maintenance_bytes_per_second = 0) {}
  virtual void PurgeSendBinding(const std::string& binding_id) {
    (void)binding_id;
  }
  virtual void RecordNetworkControlState(size_t probe_cluster_count,
                                         int64_t target_rate_bps,
                                         int64_t pacer_rate_bps,
                                         int64_t padding_rate_bps,
                                         bool has_congestion_window) {
    (void)probe_cluster_count;
    (void)target_rate_bps;
    (void)pacer_rate_bps;
    (void)padding_rate_bps;
    (void)has_congestion_window;
  }
  virtual void RecordInvalidDatagram() = 0;
  virtual size_t maximum_packet_payload(uint64_t flow_id) const {
    const auto current = stats();
    return RoqPacketPayloadLimit(current.max_datagram_payload
                                     ? current.max_datagram_payload
                                     : kRoqEarlyDatagramPayloadLimit,
                                 flow_id);
  }
  virtual void UpdatePeerDatagramPayloadLimit(size_t bytes) = 0;
  virtual void Close(uint64_t application_error = 0,
                     std::string reason = "complete") = 0;
  virtual bool WaitUntilClosed(TimeDelta timeout) = 0;
  virtual RoqSessionStats stats() const = 0;
  virtual RoqBindingSendStats send_stats(const std::string& binding_id) const {
    (void)binding_id;
    return {};
  }
  virtual std::string error() const = 0;
};

}  // namespace webrtc::roq

#endif  // API_ROQ_ROQ_SESSION_H_
