/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "api/roq/roq_session.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "modules/roq/bounded_datagram_queue.h"
#include "modules/roq/receive_flow_registry.h"
#include "modules/roq/reliable_message_framer.h"
#include "modules/roq/resumption_cache.h"
#include "modules/roq/roq_socket_waiter.h"
#include "modules/roq/roq_wire.h"
#include "quiche.h"
#include "rtc_base/crypto_random.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"

namespace webrtc::roq {

bool ShouldContinueDatagramBurst(size_t attempted,
                                 size_t limit,
                                 bool pending_pacing,
                                 size_t quiche_queue_length,
                                 size_t pending_media) {
  return attempted < limit && !pending_pacing &&
         (quiche_queue_length > 0 || pending_media > 0);
}

namespace {

using SteadyClock = std::chrono::steady_clock;
constexpr size_t kConnectionIdLength = 16;
constexpr size_t kPacketBufferSize = 65535;
constexpr uint64_t kControlStreamId = 0;
constexpr uint64_t kNoTimeout = std::numeric_limits<uint64_t>::max();
constexpr size_t kMediaBurstLimit = 64;
constexpr size_t kReceiveBurstLimit = 64;

struct ConfigDeleter {
  void operator()(quiche_config* config) const { quiche_config_free(config); }
};
struct ConnectionDeleter {
  void operator()(quiche_conn* connection) const {
    quiche_conn_free(connection);
  }
};
using ConfigPtr = std::unique_ptr<quiche_config, ConfigDeleter>;
using ConnectionPtr = std::unique_ptr<quiche_conn, ConnectionDeleter>;

bool PurgeDatagram(const uint8_t*, size_t) {
  return true;
}

class Socket {
 public:
  explicit Socket(int value = -1) : value_(value) {}
  ~Socket() {
    if (value_ >= 0)
      close(value_);
  }
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;
  Socket(Socket&& other) noexcept : value_(std::exchange(other.value_, -1)) {}
  Socket& operator=(Socket&& other) noexcept {
    if (this != &other) {
      if (value_ >= 0)
        close(value_);
      value_ = std::exchange(other.value_, -1);
    }
    return *this;
  }
  int get() const { return value_; }

 private:
  int value_;
};

struct PendingUdpPacket {
  std::vector<uint8_t> bytes;
  sockaddr_storage destination = {};
  socklen_t destination_length = 0;
  SteadyClock::time_point send_at;
  std::optional<QueuedDatagram> datagram;
};

struct ResolvedAddress {
  sockaddr_storage value = {};
  socklen_t length = 0;
};

bool SplitHostPort(const std::string& value,
                   std::string* host,
                   std::string* port,
                   std::string* error) {
  if (!value.empty() && value.front() == '[') {
    const size_t closing = value.find(']');
    if (closing == std::string::npos || closing + 1 >= value.size() ||
        value[closing + 1] != ':') {
      *error = "IPv6 UDP address must be [HOST]:PORT";
      return false;
    }
    *host = value.substr(1, closing - 1);
    *port = value.substr(closing + 2);
    return !host->empty() && !port->empty();
  }
  const size_t separator = value.rfind(':');
  if (separator == std::string::npos || value.find(':') != separator) {
    *error = "UDP address must be HOST:PORT";
    return false;
  }
  *host = value.substr(0, separator);
  *port = value.substr(separator + 1);
  return !host->empty() && !port->empty();
}

bool ResolveAddress(const std::string& value,
                    ResolvedAddress* address,
                    std::string* error) {
  std::string host;
  std::string port;
  if (!SplitHostPort(value, &host, &port, error))
    return false;
  addrinfo hints = {};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;
  addrinfo* resolved = nullptr;
  const int result = getaddrinfo(host.c_str(), port.c_str(), &hints, &resolved);
  if (result != 0 || !resolved) {
    *error =
        "failed to resolve UDP address: " + std::string(gai_strerror(result));
    return false;
  }
  std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> guard(resolved,
                                                           freeaddrinfo);
  if (resolved->ai_addrlen > sizeof(address->value)) {
    *error = "resolved UDP address is too large";
    return false;
  }
  std::memcpy(&address->value, resolved->ai_addr, resolved->ai_addrlen);
  address->length = static_cast<socklen_t>(resolved->ai_addrlen);
  return true;
}

ResolvedAddress WildcardAddress(const ResolvedAddress& remote) {
  ResolvedAddress local = remote;
  if (local.value.ss_family == AF_INET) {
    auto* address = reinterpret_cast<sockaddr_in*>(&local.value);
    address->sin_addr.s_addr = htonl(INADDR_ANY);
    address->sin_port = 0;
  } else {
    auto* address = reinterpret_cast<sockaddr_in6*>(&local.value);
    address->sin6_addr = in6addr_any;
    address->sin6_port = 0;
    address->sin6_scope_id = 0;
  }
  return local;
}

Socket OpenSocket(const ResolvedAddress& bind_address,
                  ResolvedAddress* local_address,
                  std::string* error) {
  Socket socket_fd(socket(bind_address.value.ss_family, SOCK_DGRAM, 0));
  if (socket_fd.get() < 0 ||
      bind(socket_fd.get(),
           reinterpret_cast<const sockaddr*>(&bind_address.value),
           bind_address.length) != 0 ||
      fcntl(socket_fd.get(), F_SETFL, O_NONBLOCK) != 0) {
    *error = "failed to open nonblocking UDP socket";
    return Socket();
  }
  local_address->length = sizeof(local_address->value);
  if (getsockname(socket_fd.get(),
                  reinterpret_cast<sockaddr*>(&local_address->value),
                  &local_address->length) != 0) {
    *error = "failed to read local UDP address";
    return Socket();
  }
  return socket_fd;
}

class RoqSessionImpl final : public RoqSession {
 public:
  RoqSessionImpl(RoqSessionConfig config,
                 ReceiveCallback receive_callback,
                 DatagramSizeCallback datagram_size_callback,
                 ReliableMessageCallback reliable_message_callback)
      : config_(std::move(config)),
        receive_callback_(std::move(receive_callback)),
        datagram_size_callback_(std::move(datagram_size_callback)),
        reliable_message_callback_(std::move(reliable_message_callback)),
        outgoing_(config_.outgoing_queue_capacity,
                  std::min(config_.maximum_udp_payload,
                           kRoqEarlyDatagramPayloadLimit),
                  config_.outgoing_queue_byte_capacity),
        receive_flows_(config_.receive_flow_ids.empty()
                           ? config_.allowed_flow_ids
                           : config_.receive_flow_ids,
                       config_.buffer_dynamic_receive_flows),
        send_flow_ids_(config_.allowed_flow_ids) {
    stats_.max_datagram_payload =
        std::min(config_.maximum_udp_payload, kRoqEarlyDatagramPayloadLimit);
  }

  ~RoqSessionImpl() override {
    Close(0, "complete");
    if (thread_.joinable())
      thread_.join();
    RTC_LOG(LS_INFO) << "roq_transport_summary oversize_drops="
                     << stats_.oversize_drops
                     << " pmtu_drops=" << stats_.pmtu_purge_drops
                     << " early_datagrams_sent=" << stats_.early_datagrams_sent
                     << " received_datagrams=" << stats_.received_datagrams;
  }

  bool Start(std::string* error) override {
    std::lock_guard lock(mutex_);
    if (thread_.joinable()) {
      *error = "RoQ session is already started";
      return false;
    }
    if (config_.allowed_flow_ids.empty()) {
      *error = "at least one RoQ flow ID is required";
      return false;
    }
    if (waiter_.error()) {
      *error = "failed to initialize network event waiter: " +
               std::to_string(waiter_.error());
      return false;
    }
    thread_ = std::thread([this] { Run(); });
    return true;
  }

  bool WaitUntilReady(TimeDelta timeout) override {
    std::unique_lock lock(mutex_);
    return condition_.wait_for(lock, std::chrono::microseconds(timeout.us()),
                               [this] { return ready_ || !error_.empty(); }) &&
           ready_;
  }

  bool WaitUntilEstablished(TimeDelta timeout) override {
    std::unique_lock lock(mutex_);
    return condition_.wait_for(
               lock, std::chrono::microseconds(timeout.us()),
               [this] { return stats_.established || !error_.empty(); }) &&
           stats_.established;
  }

  bool SendReplaySafeMessage(std::span<const uint8_t> message) override {
    std::vector<uint8_t> framed = FrameReliableMessage(message);
    if (framed.empty())
      return false;
    std::lock_guard lock(mutex_);
    if (close_requested_ || closed_)
      return false;
    reliable_messages_.push_back(std::move(framed));
    WakeNetworkLocked();
    return true;
  }

  void SetEarlyDatagramsAllowed(bool allowed) override {
    std::lock_guard lock(mutex_);
    early_datagrams_allowed_ = allowed;
    WakeNetworkLocked();
    if (!allowed && !stats_.established) {
      stats_.early_rejection_queue_drops += outgoing_.Clear();
      purge_quiche_slot_for_binding_ = true;
      CopyQueueStatsLocked();
    }
  }

  void RequestPing() override {
    std::lock_guard lock(mutex_);
    ping_requested_ = true;
    WakeNetworkLocked();
  }

  void Abort() override {
    std::lock_guard lock(mutex_);
    abort_requested_ = true;
    WakeNetworkLocked();
    condition_.notify_all();
  }

  bool WaitUntilReliableMessagesSent(size_t count, TimeDelta timeout) override {
    std::unique_lock lock(mutex_);
    return condition_.wait_for(
        lock, std::chrono::microseconds(timeout.us()), [this, count] {
          return stats_.reliable_messages_sent >= count || closed_;
        });
  }

  bool WaitUntilReliableMessagesDrained(TimeDelta timeout) override {
    std::unique_lock lock(mutex_);
    return condition_.wait_for(
        lock, std::chrono::microseconds(timeout.us()),
        [this] { return reliable_messages_.empty() || closed_; });
  }

  void BeginReceiveFlowUpdate() override {
    std::lock_guard lock(mutex_);
    receive_flows_.BeginUpdate(config_.clock->CurrentTime());
  }

  void UpdateReceiveFlowIds(std::vector<uint64_t> flow_ids) override {
    std::lock_guard lock(mutex_);
    QueueReceivedLocked(receive_flows_.Update(std::move(flow_ids),
                                              config_.clock->CurrentTime()));
    CopyFlowStatsLocked();
    WakeNetworkLocked();
  }

  void BeginFlowUpdate() override { BeginReceiveFlowUpdate(); }

  void PrepareReceiveFlowIds(std::vector<uint64_t> flow_ids) override {
    std::lock_guard lock(mutex_);
    receive_flows_.Prepare(std::move(flow_ids));
  }

  void CommitFlowUpdate(std::vector<RoqFlowConfig> flows) override {
    std::vector<ReceiveFlowConfig> receive_flows;
    std::vector<uint64_t> send_flows;
    receive_flows.reserve(flows.size());
    for (const RoqFlowConfig& flow : flows) {
      receive_flows.push_back(
          {flow.flow_id, flow.receive_policy == RoqReceiveFlowPolicy::kDeliver
                             ? ReceiveFlowPolicy::kDeliver
                             : ReceiveFlowPolicy::kDiscard});
      if (flow.allow_send)
        send_flows.push_back(flow.flow_id);
    }
    size_t packet_limit;
    {
      std::lock_guard lock(mutex_);
      receive_generations_.clear();
      for (const auto& flow : flows)
        receive_generations_[flow.flow_id] = flow.binding_generation;
      QueueReceivedLocked(receive_flows_.Update(std::move(receive_flows),
                                                config_.clock->CurrentTime()));
      send_flow_ids_ = std::move(send_flows);
      packet_limit = PacketLimitLocked(stats_.max_datagram_payload);
      CopyFlowStatsLocked();
      WakeNetworkLocked();
    }
    if (datagram_size_callback_)
      datagram_size_callback_(packet_limit);
  }

  void QueueReceivedLocked(std::vector<RoqReceivedPacket> packets) {
    for (auto& packet : packets) {
      packet.binding_generation = receive_generations_[packet.flow_id];
      if (pending_received_.size() >= 512 ||
          pending_received_bytes_ + packet.packet.size() > 512 * 1024)
        continue;
      pending_received_bytes_ += packet.packet.size();
      pending_received_.push_back(std::move(packet));
    }
  }

  void DispatchReceived() {
    std::deque<RoqReceivedPacket> packets;
    {
      std::lock_guard lock(mutex_);
      packets.swap(pending_received_);
      pending_received_bytes_ = 0;
    }
    if (receive_callback_)
      for (auto& packet : packets) {
        const auto now_us =
            std::chrono::duration_cast<std::chrono::microseconds>(
                SteadyClock::now().time_since_epoch())
                .count();
        if (packet.received_at_us < 0 ||
            now_us - packet.received_at_us <= 250000)
          receive_callback_(std::move(packet));
      }
  }

  RoqSendResult SendMedia(uint64_t flow_id,
                          std::span<const uint8_t> packet,
                          RoqDatagramMetadata metadata,
                          std::function<void()> on_submitted) override {
    std::lock_guard lock(mutex_);
    if (close_requested_ || closed_)
      return RoqSendResult::kClosed;
    if (std::find(send_flow_ids_.begin(), send_flow_ids_.end(), flow_id) ==
        send_flow_ids_.end()) {
      return RoqSendResult::kClosed;
    }
    QueuedDatagram datagram = {
        .bytes = EncodeRoqDatagram(flow_id, packet),
        .binding_id = metadata.binding_id,
        .flow_id = flow_id,
        .media_type = metadata.media_type,
        .enqueue_time = metadata.enqueue_time,
        .deadline = metadata.deadline,
        .priority = metadata.priority,
        .frame_id = metadata.frame_id,
        .packet_id = metadata.packet_id,
        .original_packet_id = metadata.original_packet_id,
        .frame_complete = metadata.frame_complete,
        .video_codec = metadata.video_codec,
        .is_key = metadata.is_key,
        .contains_vps = metadata.contains_vps,
        .contains_sps = metadata.contains_sps,
        .contains_pps = metadata.contains_pps,
        .on_submitted = std::move(on_submitted),
        .rtp_timestamp = metadata.rtp_timestamp,
        .primary_payload_bytes = metadata.primary_payload_bytes,
        .periodic_rtcp_key = std::move(metadata.periodic_rtcp_key),
        .completion = std::move(metadata.completion),
    };
    // 96 bytes conservatively covers QUIC short header/AEAD/DATAGRAM +
    // UDP/IPv6. RED redundancy and RTP headers are measured, not nominal codec
    // bitrate.
    const uint64_t cost = datagram.bytes.size() + 96;
    if (metadata.original_packet_id) {
      stats_.recovery_wire_bytes += cost;
    } else if (metadata.media_type == RoqMediaType::kAudio) {
      stats_.audio_payload_bytes += metadata.primary_payload_bytes;
      stats_.audio_wire_bytes += cost;
      ++stats_.audio_packets;
      stats_.audio_redundant_payload_bytes += metadata.redundant_payload_bytes;
      stats_.audio_redundant_blocks += metadata.redundant_blocks;
    } else if (metadata.media_type == RoqMediaType::kVideo) {
      stats_.video_payload_bytes += metadata.primary_payload_bytes;
      stats_.video_wire_bytes += cost;
    } else {
      stats_.rtcp_wire_bytes += cost;
    }
    const DatagramEnqueueResult result =
        outgoing_.Enqueue(std::move(datagram), config_.clock->CurrentTime());
    CopyQueueStatsLocked();
    switch (result) {
      case DatagramEnqueueResult::kQueued:
        WakeNetworkLocked();
        return RoqSendResult::kQueued;
      case DatagramEnqueueResult::kDropped:
        return RoqSendResult::kDropped;
      case DatagramEnqueueResult::kQueueFull:
        return RoqSendResult::kQueueFull;
      case DatagramEnqueueResult::kOversize:
        return RoqSendResult::kOversize;
      case DatagramEnqueueResult::kInvalid:
        return RoqSendResult::kInvalid;
    }
  }

  void SetMediaWireBudget(uint64_t probe_max_bytes_per_second,
                          uint64_t recovery_bytes_per_second,
                          uint64_t maintenance_bytes_per_second) override {
    std::lock_guard lock(mutex_);
    probe_max_bytes_per_second_ = probe_max_bytes_per_second;
    maintenance_max_bytes_per_second_ = maintenance_bytes_per_second;
    outgoing_.SetRecoveryRate(recovery_bytes_per_second,
                              config_.clock->CurrentTime());
    WakeNetworkLocked();
  }

  void RecordInvalidDatagram() override {
    std::lock_guard lock(mutex_);
    outgoing_.RecordInvalid();
    CopyQueueStatsLocked();
  }

  void PurgeSendBinding(const std::string& binding_id) override {
    std::lock_guard lock(mutex_);
    outgoing_.PurgeBinding(binding_id);
    if (quiche_slot_datagram_ &&
        quiche_slot_datagram_->binding_id == binding_id)
      purge_quiche_slot_for_binding_ = true;
    CopyQueueStatsLocked();
    WakeNetworkLocked();
  }

  void RecordNetworkControlState(size_t probe_cluster_count,
                                 int64_t target_rate_bps,
                                 int64_t pacer_rate_bps,
                                 int64_t padding_rate_bps,
                                 bool has_congestion_window) override {
    std::lock_guard lock(mutex_);
    ++stats_.network_control_updates;
    stats_.controller_probe_cluster_count = probe_cluster_count;
    stats_.controller_target_rate_bps = target_rate_bps;
    stats_.controller_pacer_rate_bps = pacer_rate_bps;
    stats_.controller_padding_rate_bps = padding_rate_bps;
    stats_.controller_has_congestion_window = has_congestion_window;
  }

  size_t maximum_packet_payload(uint64_t flow_id) const override {
    std::lock_guard lock(mutex_);
    return RoqPacketPayloadLimit(stats_.max_datagram_payload, flow_id);
  }

  size_t PacketLimitLocked(size_t datagram_bytes) const {
    size_t result = datagram_bytes;
    for (uint64_t flow : send_flow_ids_)
      result = std::min(result, RoqPacketPayloadLimit(datagram_bytes, flow));
    return result;
  }

  void UpdatePeerDatagramPayloadLimit(size_t bytes) override {
    DatagramSizeCallback callback;
    size_t packet_limit = 0;
    {
      std::lock_guard lock(mutex_);
      application_datagram_limit_ = bytes;
      const size_t effective =
          stats_.max_datagram_payload == 0
              ? bytes
              : std::min(bytes, stats_.max_datagram_payload);
      stats_.max_datagram_payload = effective;
      outgoing_.SetMaxDatagramSize(effective, config_.clock->CurrentTime());
      if (quiche_slot_datagram_ &&
          quiche_slot_datagram_->bytes.size() > effective)
        purge_quiche_slot_for_pmtu_ = true;
      CopyQueueStatsLocked();
      callback = datagram_size_callback_;
      packet_limit = PacketLimitLocked(effective);
      WakeNetworkLocked();
    }
    if (callback && packet_limit > 0)
      callback(packet_limit);
  }

  void Close(uint64_t application_error, std::string reason) override {
    std::lock_guard lock(mutex_);
    close_requested_ = true;
    outgoing_.Clear();
    CopyQueueStatsLocked();
    close_code_ = application_error;
    close_reason_ = std::move(reason);
    WakeNetworkLocked();
  }

  bool WaitUntilClosed(TimeDelta timeout) override {
    std::unique_lock lock(mutex_);
    return condition_.wait_for(lock, std::chrono::microseconds(timeout.us()),
                               [this] { return closed_; });
  }

  RoqSessionStats stats() const override {
    std::lock_guard lock(mutex_);
    return stats_;
  }

  RoqBindingSendStats send_stats(const std::string& binding_id) const override {
    std::lock_guard lock(mutex_);
    const DatagramBindingStats stats = outgoing_.BindingStats(binding_id);
    return {.enqueued_packets = stats.enqueued_packets,
            .queued_packets = stats.queued_packets,
            .queued_bytes = stats.queued_bytes,
            .submitted_packets = stats.submitted_packets,
            .dropped_packets = stats.dropped_packets,
            .dependency_drops = stats.dependency_drops,
            .recovery_requests = stats.recovery_requests};
  }

  std::string error() const override {
    std::lock_guard lock(mutex_);
    return error_;
  }

 private:
  void TrimVideoQueue(quiche_conn* connection) {
    std::lock_guard lock(mutex_);
    // A zero quiche queue means the slot has already been packetized. The
    // pending UDP payload is immutable and must not be treated as queued RTP.
    if (quiche_conn_dgram_send_queue_len(connection) == 0) {
      quiche_slot_datagram_.reset();
      purge_quiche_slot_for_binding_ = false;
      purge_quiche_slot_for_pmtu_ = false;
    } else if (purge_quiche_slot_for_binding_ || purge_quiche_slot_for_pmtu_) {
      quiche_conn_dgram_purge_outgoing(connection, PurgeDatagram);
      if (purge_quiche_slot_for_pmtu_)
        ++quiche_pmtu_purge_drops_;
      if (!purge_quiche_slot_for_binding_ && quiche_slot_datagram_)
        outgoing_.DiscardFrame(*quiche_slot_datagram_,
                               config_.clock->CurrentTime());
      quiche_slot_datagram_.reset();
      purge_quiche_slot_for_binding_ = false;
      purge_quiche_slot_for_pmtu_ = false;
    }
    const auto now = config_.clock->CurrentTime();
    if (outgoing_.TrimVideo(
            now, quiche_slot_datagram_ ? &*quiche_slot_datagram_ : nullptr)) {
      quiche_conn_dgram_purge_outgoing(connection, PurgeDatagram);
      quiche_slot_datagram_.reset();
      ++stats_.quiche_slot_expired_drops;
    }
    CopyQueueStatsLocked();
  }

  void WakeNetworkLocked() {
    if (!waiter_.WakeUp()) {
      if (error_.empty())
        error_ = "failed to wake network event waiter";
      abort_requested_ = true;
      condition_.notify_all();
    }
  }

  void Fail(std::string error) {
    std::string failure;
    {
      std::lock_guard lock(mutex_);
      if (error_.empty())
        error_ = std::move(error);
      failure = error_;
      outgoing_.Clear();
      quiche_slot_datagram_.reset();
      closed_ = true;
      condition_.notify_all();
    }
    NotifyLifecycle(RoqSessionLifecycleState::kClosed, std::move(failure));
  }

  void NotifyLifecycle(RoqSessionLifecycleState state, std::string error = {}) {
    if (config_.lifecycle_callback)
      config_.lifecycle_callback({.state = state, .error = std::move(error)});
  }

  ConfigPtr CreateConfig(std::string* error) {
    ConfigPtr config(quiche_config_new(QUICHE_PROTOCOL_VERSION));
    std::vector<uint8_t> alpn_wire;
    if (config_.alpn.size() > 255) {
      *error = "ALPN is too long";
      return nullptr;
    }
    alpn_wire.push_back(static_cast<uint8_t>(config_.alpn.size()));
    alpn_wire.insert(alpn_wire.end(), config_.alpn.begin(), config_.alpn.end());
    if (!config ||
        quiche_config_set_application_protos(config.get(), alpn_wire.data(),
                                             alpn_wire.size()) != 0 ||
        quiche_config_set_cc_algorithm_name(config.get(), "bbr2_gcongestion") != 0 ||
        quiche_config_set_bbr_loss_response(config.get(), 1) != 0) {
      *error = "failed to configure quiche";
      return nullptr;
    }
    quiche_config_set_max_idle_timeout(config.get(),
                                       config_.max_idle_timeout_ms);
    quiche_config_set_max_recv_udp_payload_size(config.get(),
                                                config_.maximum_udp_payload);
    quiche_config_set_max_send_udp_payload_size(config.get(),
                                                config_.maximum_udp_payload);
    quiche_config_set_initial_max_data(config.get(), 1000000);
    quiche_config_set_initial_max_stream_data_bidi_local(config.get(), 10000);
    quiche_config_set_initial_max_stream_data_bidi_remote(config.get(), 10000);
    quiche_config_set_initial_max_streams_bidi(config.get(), 4);
    quiche_config_enable_early_data(config.get());
    quiche_config_enable_dgram(config.get(), true, 1024, 1);
    quiche_config_verify_peer(config.get(), true);
    if ((!config_.trusted_ca_file.empty() &&
         quiche_config_load_verify_locations_from_file(
             config.get(), config_.trusted_ca_file.c_str()) != 0) ||
        (!config_.trusted_ca_directory.empty() &&
         quiche_config_load_verify_locations_from_directory(
             config.get(), config_.trusted_ca_directory.c_str()) != 0)) {
      *error = "failed to load trusted CA locations";
      return nullptr;
    }
    return config;
  }

  void ObserveSendQueue(quiche_conn* connection) {
    std::lock_guard lock(mutex_);
    const auto age = outgoing_.OldestVideoAge(
        config_.clock->CurrentTime(),
        quiche_slot_datagram_ ? &*quiche_slot_datagram_ : nullptr);
    quiche_conn_observe_send_queue(connection, outgoing_.byte_size(),
                                  std::max<int64_t>(0, age.us()) * 1000,
                                  outgoing_.expired_video_frames() +
                                      outgoing_.queue_full_drops());
  }

  void CopyQueueStatsLocked() {
    stats_.queue_full_drops = outgoing_.queue_full_drops();
    stats_.oversize_drops = outgoing_.oversize_drops() + quiche_oversize_drops_;
    stats_.pmtu_purge_drops =
        outgoing_.pmtu_purge_drops() + quiche_pmtu_purge_drops_;
    stats_.expired_drops = outgoing_.expired_drops();
    stats_.expired_video_frames = outgoing_.expired_video_frames();
    stats_.oldest_video_queue_age_us =
        outgoing_
            .OldestVideoAge(
                config_.clock->CurrentTime(),
                quiche_slot_datagram_ ? &*quiche_slot_datagram_ : nullptr)
            .us();
    stats_.max_video_queue_age_us = outgoing_.max_video_queue_age().us();
    stats_.invalid_drops = outgoing_.invalid_drops();
    stats_.peak_queue_size = outgoing_.peak_size();
    stats_.peak_queue_bytes = outgoing_.peak_byte_size();
    stats_.queued_packets = outgoing_.size();
    stats_.queued_bytes = outgoing_.byte_size();
    stats_.max_queue_delay_us = outgoing_.max_queue_delay().us();
    stats_.dropped_periodic_rtcp =
        outgoing_.dropped_by_priority(DatagramPriority::kPeriodicRtcp);
    stats_.dropped_video_delta =
        outgoing_.dropped_by_priority(DatagramPriority::kVideoDelta);
    stats_.dropped_video_key =
        outgoing_.dropped_by_priority(DatagramPriority::kVideoKey);
    stats_.dropped_audio =
        outgoing_.dropped_by_priority(DatagramPriority::kAudio);
    stats_.dropped_critical_rtcp =
        outgoing_.dropped_by_priority(DatagramPriority::kRtcp);
    stats_.dependency_drops = outgoing_.dependency_drops();
    stats_.recovery_requests = outgoing_.recovery_requests();
    stats_.recovery_budget_drops = outgoing_.recovery_budget_drops();
    stats_.coalesced_reports = outgoing_.coalesced_reports();
  }

  void CopyFlowStatsLocked() {
    stats_.unknown_flow_buffered = receive_flows_.buffered();
    stats_.unknown_flow_released = receive_flows_.released();
    stats_.unknown_flow_dropped = receive_flows_.dropped();
  }

  void UpdatePathStats(quiche_conn* connection) {
    quiche_path_stats path = {};
    quiche_stats connection_stats = {};
    if (quiche_conn_path_stats(connection, 0, &path) != 0)
      return;
    quiche_conn_stats(connection, &connection_stats);
    quiche_bandwidth_recovery_stats recovery = {};
    quiche_conn_bandwidth_recovery_stats(connection, &recovery);
    {
      std::lock_guard lock(mutex_);
      stats_.probe_wire_bytes =
          quiche_conn_bandwidth_probe_sent_bytes(connection);
      stats_.sent_wire_bytes = connection_stats.sent_bytes;
      stats_.bandwidth_evidence_round = recovery.evidence_round;
      stats_.bandwidth_evidence_bytes_per_second = recovery.evidence_rate;
      stats_.bandwidth_evidence_age_ns = recovery.evidence_age_ns;
      stats_.maintenance_wire_bytes = recovery.maintenance_sent_bytes;
      stats_.rtt_ns = path.rtt;
      stats_.max_rtt_ns = path.max_rtt;
      stats_.delivery_rate_bytes_per_second = path.delivery_rate;
      stats_.max_bandwidth_bytes_per_second = path.max_bandwidth;
      stats_.acked_bytes = connection_stats.acked_bytes;
      stats_.startup_exit_cwnd = path.startup_exit_cwnd;
      stats_.current_cwnd_bytes = path.cwnd;
      stats_.lost_packets = connection_stats.lost;
      stats_.lost_bytes = connection_stats.lost_bytes;
      stats_.pmtu_bytes = path.pmtu;
      stats_.dgram_lost = path.dgram_lost;
      if (connection_stats.acked_bytes > last_path_acked_bytes_) {
        last_path_acked_bytes_ = connection_stats.acked_bytes;
        stats_.last_ack_monotonic_us =
            std::chrono::duration_cast<std::chrono::microseconds>(
                SteadyClock::now().time_since_epoch())
                .count();
      }
    }
    LogTransportDiagnostics(connection);
  }

  void LogTransportDiagnostics(quiche_conn* connection) {
    if (!media_diagnostics_)
      return;
    const auto now = config_.clock->CurrentTime();
    if (now - last_diagnostic_at_ < TimeDelta::Millis(500))
      return;
    last_diagnostic_at_ = now;
    RoqSessionStats stats;
    DatagramQueueDiagnostics classes;
    uint64_t probe_cap = 0;
    {
      std::lock_guard lock(mutex_);
      stats = stats_;
      probe_cap = probe_max_bytes_per_second_;
      classes = outgoing_.Diagnostics(
          now, quiche_conn_dgram_send_queue_len(connection) > 0 &&
                       quiche_slot_datagram_
                   ? &*quiche_slot_datagram_
                   : nullptr);
    }
    // Copy under the queue lock, but format/write after releasing it. Logging
    // is opt-in and sampled, independent of subscriber/renderer availability.
    RTC_LOG(LS_INFO) << "roq_transport_sample sample_us=" << TimeMicros()
                     << " session=" << this
                     << " established=" << stats.established
                     << " rtt_us=" << stats.rtt_ns / 1000 << " delivery_bps="
                     << stats.delivery_rate_bytes_per_second * 8
                     << " max_bw_bps="
                     << stats.max_bandwidth_bytes_per_second * 8
                     << " cwnd_bytes=" << stats.current_cwnd_bytes
                     << " acked_bytes=" << stats.acked_bytes
                     << " lost_packets=" << stats.lost_packets
                     << " dgram_lost=" << stats.dgram_lost
                     << " sent_udp=" << stats.sent_udp_packets
                     << " received_udp=" << stats.received_udp_packets
                     << " startup_exit_cwnd=" << stats.startup_exit_cwnd
                     << " controller_bps=" << stats.controller_target_rate_bps
                     << " queued_packets=" << stats.queued_packets
                     << " queued_bytes=" << stats.queued_bytes
                     << " queue_full=" << stats.queue_full_drops
                     << " expired=" << stats.expired_drops
                     << " expired_frames=" << stats.expired_video_frames
                     << " recovery_requests=" << stats.recovery_requests
                     << " slot_expired=" << stats.quiche_slot_expired_drops
                     << " probe_cap_Bps=" << probe_cap
                     << " probe_bytes=" << stats.probe_wire_bytes
                     << " maintenance_bytes=" << stats.maintenance_wire_bytes
                     << " evidence_round=" << stats.bandwidth_evidence_round
                     << " evidence_bps=" << stats.bandwidth_evidence_bytes_per_second * 8
                     << " evidence_age_ns=" << stats.bandwidth_evidence_age_ns
                     << " recovery_budget_drops=" << stats.recovery_budget_drops
                     << " coalesced_reports=" << stats.coalesced_reports
                     << " audio_wire_bytes=" << stats.audio_wire_bytes
                     << " audio_primary_bytes=" << stats.audio_payload_bytes
                     << " audio_packets=" << stats.audio_packets
                     << " audio_redundant_bytes="
                     << stats.audio_redundant_payload_bytes
                     << " audio_red_blocks=" << stats.audio_redundant_blocks
                     << " video_wire_bytes=" << stats.video_wire_bytes;
    constexpr std::array<const char*, 6> names = {
        "periodic_rtcp", "video_delta", "video_key",
        "rtx",           "audio",       "critical_rtcp"};
    for (size_t i = 0; i < classes.size(); ++i) {
      const auto& c = classes[i];
      RTC_LOG(LS_INFO) << "roq_transport_class sample_us=" << TimeMicros()
                       << " session=" << this << " class=" << names[i]
                       << " enqueued_packets=" << c.enqueued_packets
                       << " enqueued_bytes=" << c.enqueued_bytes
                       << " submitted_packets=" << c.submitted_packets
                       << " submitted_bytes=" << c.submitted_bytes
                       << " dropped_packets=" << c.dropped_packets
                       << " dropped_bytes=" << c.dropped_bytes
                       << " purged_packets=" << c.purged_packets
                       << " queued_packets=" << c.queued_packets
                       << " queued_bytes=" << c.queued_bytes
                       << " slot_packets=" << c.slot_packets
                       << " oldest_age_us=" << c.oldest_age_us;
    }
  }

  void UpdateDatagramLimit(quiche_conn* connection) {
    const ssize_t writable = quiche_conn_dgram_max_writable_len(connection);
    if (writable <= 1)
      return;
    DatagramSizeCallback callback;
    size_t packet_limit = 0;
    {
      std::lock_guard lock(mutex_);
      const size_t effective = application_datagram_limit_
                                   ? std::min(static_cast<size_t>(writable),
                                              *application_datagram_limit_)
                                   : static_cast<size_t>(writable);
      if (stats_.max_datagram_payload == effective)
        return;
      stats_.max_datagram_payload = effective;
      outgoing_.SetMaxDatagramSize(effective, config_.clock->CurrentTime());
      if (quiche_slot_datagram_ &&
          quiche_slot_datagram_->bytes.size() > effective)
        purge_quiche_slot_for_pmtu_ = true;
      CopyQueueStatsLocked();
      callback = datagram_size_callback_;
      packet_limit = PacketLimitLocked(effective);
    }
    if (callback)
      callback(packet_limit);
  }

  bool DrainReceived(quiche_conn* connection) {
    std::vector<uint8_t> buffer;
    while (true) {
      const ssize_t length = quiche_conn_dgram_recv_front_len(connection);
      if (length == QUICHE_ERR_DONE)
        return true;
      if (length < 0)
        return false;
      buffer.resize(static_cast<size_t>(length));
      const ssize_t read =
          quiche_conn_dgram_recv(connection, buffer.data(), buffer.size());
      if (read < 0)
        return false;
      RoqDatagram decoded;
      std::string error;
      if (!DecodeRoqDatagram(std::span<const uint8_t>(
                                 buffer.data(), static_cast<size_t>(read)),
                             &decoded, &error)) {
        quiche_conn_close(
            connection, true, static_cast<uint64_t>(RoqError::kPacketError),
            reinterpret_cast<const uint8_t*>(error.data()), error.size());
        return true;
      }
      RoqReceivedPacket received = {
          decoded.flow_id,
          std::vector<uint8_t>(decoded.packet.begin(), decoded.packet.end()),
          std::chrono::duration_cast<std::chrono::microseconds>(
              SteadyClock::now().time_since_epoch())
              .count()};
      ReceiveFlowRoute route;
      {
        std::lock_guard lock(mutex_);
        route = receive_flows_.Route(received, config_.clock->CurrentTime());
        if (route == ReceiveFlowRoute::kDeliver) {
          ++stats_.received_datagrams;
          if (!stats_.established)
            ++stats_.early_datagrams_received;
          std::vector<RoqReceivedPacket> ready;
          ready.push_back(std::move(received));
          QueueReceivedLocked(std::move(ready));
        }
        CopyFlowStatsLocked();
      }
      if (route == ReceiveFlowRoute::kClose) {
        constexpr std::string_view reason = "unknown RoQ flow ID";
        quiche_conn_close(
            connection, true, static_cast<uint64_t>(RoqError::kUnknownFlowId),
            reinterpret_cast<const uint8_t*>(reason.data()), reason.size());
        return true;
      }
      if (route != ReceiveFlowRoute::kDeliver)
        continue;
    }
  }

  bool DrainReliableMessages(quiche_conn* connection) {
    std::array<uint8_t, 4096> buffer = {};
    while (quiche_conn_stream_readable(connection, kControlStreamId)) {
      bool fin = false;
      uint64_t error_code = 0;
      const ssize_t read =
          quiche_conn_stream_recv(connection, kControlStreamId, buffer.data(),
                                  buffer.size(), &fin, &error_code);
      if (read == QUICHE_ERR_DONE)
        break;
      if (read < 0 || !reliable_decoder_.Push(
                          std::span<const uint8_t>(buffer.data(), read))) {
        return false;
      }
      for (std::vector<uint8_t>& message : reliable_decoder_.TakeMessages()) {
        ReliableMessageCallback callback;
        {
          std::lock_guard lock(mutex_);
          ++stats_.reliable_messages_received;
          if (!quiche_conn_is_established(connection))
            ++stats_.early_reliable_messages_received;
          callback = reliable_message_callback_;
        }
        if (callback)
          callback(std::move(message));
      }
    }
    return true;
  }

  bool SendReliableMessages(quiche_conn* connection) {
    std::lock_guard lock(mutex_);
    while (!reliable_messages_.empty()) {
      std::vector<uint8_t>& message = reliable_messages_.front();
      const std::span<const uint8_t> remaining(message);
      uint64_t error_code = 0;
      const ssize_t written = quiche_conn_stream_send(
          connection, kControlStreamId,
          remaining.subspan(reliable_message_offset_).data(),
          remaining.size() - reliable_message_offset_, false, &error_code);
      if (written == QUICHE_ERR_DONE)
        return true;
      if (written < 0)
        return false;
      reliable_message_offset_ += static_cast<size_t>(written);
      if (quiche_conn_is_in_early_data(connection))
        stats_.early_data_attempted = true;
      if (reliable_message_offset_ != message.size())
        return true;
      reliable_messages_.pop_front();
      reliable_message_offset_ = 0;
      ++stats_.reliable_messages_sent;
      condition_.notify_all();
    }
    return true;
  }

  void Run() {
    std::string setup_error;
    ResolvedAddress configured_address;
    if (!ResolveAddress(config_.address, &configured_address, &setup_error)) {
      Fail(std::move(setup_error));
      return;
    }
    const ResolvedAddress bind_address = WildcardAddress(configured_address);
    ResolvedAddress local_address;
    Socket socket_fd = OpenSocket(bind_address, &local_address, &setup_error);
    if (socket_fd.get() < 0) {
      Fail(std::move(setup_error));
      return;
    }
    if (!waiter_.AttachSocket(socket_fd.get())) {
      Fail("failed to attach UDP socket to event waiter: " +
           std::to_string(waiter_.error()));
      return;
    }
    ConfigPtr quiche_config = CreateConfig(&setup_error);
    if (!quiche_config) {
      Fail(std::move(setup_error));
      return;
    }
    std::string random_connection_id;
    if (!CreateRandomData(kConnectionIdLength, &random_connection_id)) {
      Fail("failed to generate client connection ID");
      return;
    }
    const std::span<const uint8_t> scid(
        reinterpret_cast<const uint8_t*>(random_connection_id.data()),
        random_connection_id.size());
    {
      std::lock_guard lock(mutex_);
      stats_.source_connection_id.assign(scid.begin(), scid.end());
    }
    ConnectionPtr connection(quiche_connect(
        config_.server_name.c_str(), scid.data(), scid.size(),
        reinterpret_cast<const sockaddr*>(&local_address.value),
        local_address.length,
        reinterpret_cast<const sockaddr*>(&configured_address.value),
        configured_address.length, quiche_config.get()));
    if (!connection) {
      Fail("failed to create client QUIC connection");
      return;
    }
    const ResumptionCacheKey cache_key = {
        .endpoint = config_.address,
        .server_name = config_.server_name,
        .alpn = config_.alpn,
        .context = config_.resumption_context,
    };
    std::unique_ptr<ResumptionCache> cache;
    std::vector<uint8_t> cached_address_token;
    if (!config_.resumption_cache_directory.empty()) {
      cache = std::make_unique<ResumptionCache>(
          config_.resumption_cache_directory, config_.resumption_cache_max_age);
      std::string cache_error;
      ResumptionCacheLoadStatus cache_status;
      const Timestamp cache_now = config_.resumption_clock
                                      ? config_.resumption_clock()
                                      : Timestamp::Micros(TimeUTCMicros());
      const std::optional<ResumptionState> cached =
          cache->Load(cache_key, cache_now, &cache_error, &cache_status);
      {
        std::lock_guard lock(mutex_);
        stats_.resumption_cache_hit = cached.has_value();
        stats_.resumption_cache_expired =
            cache_status == ResumptionCacheLoadStatus::kExpired;
        stats_.resumption_cache_invalid =
            cache_status == ResumptionCacheLoadStatus::kInvalid;
      }
      if (!cache_error.empty())
        RTC_LOG(LS_WARNING) << cache_error;
      if (quiche_conn_set_application_early_data_context(
              connection.get(), config_.resumption_context.data(),
              config_.resumption_context.size()) != 0) {
        Fail("failed to configure QUIC resumption context");
        return;
      }
      if (cached && !cached->session.empty()) {
        if (quiche_conn_set_session(connection.get(), cached->session.data(),
                                    cached->session.size()) != 0) {
          Fail("failed to restore cached QUIC session");
          return;
        }
        if (!cached->address_token.empty() &&
            quiche_conn_set_token(connection.get(),
                                  cached->address_token.data(),
                                  cached->address_token.size()) != 0) {
          Fail("failed to restore cached address token");
          return;
        }
        cached_address_token = cached->address_token;
        // The early budget is installed before producers can enqueue.
      }
    }
    receive_flows_.BeginUpdate(config_.clock->CurrentTime());
    {
      std::lock_guard lock(mutex_);
      ready_ = true;
      condition_.notify_all();
    }
    NotifyLifecycle(RoqSessionLifecycleState::kReady);

    std::optional<PendingUdpPacket> pending_udp;
    bool socket_write_blocked = false;
    auto send_or_defer_udp = [&](PendingUdpPacket pending) {
      const auto send_now = SteadyClock::now();
      if (pending.send_at > send_now) {
        pending_udp = std::move(pending);
        return true;
      }
      ssize_t sent;
      do {
        sent =
            sendto(socket_fd.get(), pending.bytes.data(), pending.bytes.size(),
                   0, reinterpret_cast<const sockaddr*>(&pending.destination),
                   pending.destination_length);
      } while (sent < 0 && errno == EINTR);
      const int send_error = sent < 0 ? errno : 0;
      const RoqUdpSendDisposition disposition =
          ClassifyUdpSendResult(sent, pending.bytes.size(), send_error);
      if (disposition == RoqUdpSendDisposition::kRetry) {
        socket_write_blocked =
            send_error == EAGAIN || send_error == EWOULDBLOCK;
        // ENOBUFS need not make the socket non-writable. Back off on a timer;
        // EAGAIN waits for the kernel's writable notification.
        if (!socket_write_blocked)
          pending.send_at = SteadyClock::now() + std::chrono::milliseconds(1);
        pending_udp = std::move(pending);
        return true;
      }
      if (disposition == RoqUdpSendDisposition::kFatal) {
        Fail("UDP send failed");
        return false;
      }
      const auto submitted_at = config_.clock->CurrentTime();
      {
        std::lock_guard lock(mutex_);
        ++stats_.sent_udp_packets;
        if (pending.datagram) {
          outgoing_.RecordSubmitted(*pending.datagram, submitted_at);
          ++stats_.sent_datagrams;
          if (!stats_.established) {
            ++stats_.early_datagrams_sent;
            stats_.early_data_attempted = true;
          }
        }
      }
      if (pending.datagram) {
        if (pending.datagram->completion)
          pending.datagram->completion->Complete(true, submitted_at.us());
        if (pending.datagram->on_submitted)
          pending.datagram->on_submitted();
      }
      return true;
    };
    bool local_close_sent = false;
    bool close_drained = false;
    std::array<uint8_t, kPacketBufferSize> packet = {};
    while (true) {
      {
        std::lock_guard lock(mutex_);
        if (abort_requested_)
          break;
      }
      bool send_progress = false;
      size_t received_in_batch = 0;
      while (received_in_batch < kReceiveBurstLimit) {
        sockaddr_storage peer_address = {};
        socklen_t peer_length = sizeof(peer_address);
        const ssize_t received =
            recvfrom(socket_fd.get(), packet.data(), packet.size(), 0,
                     reinterpret_cast<sockaddr*>(&peer_address), &peer_length);
        if (received < 0) {
          if (errno == EINTR)
            continue;
          if (errno != EAGAIN && errno != EWOULDBLOCK) {
            Fail("UDP receive failed");
            return;
          }
          break;
        }
        ++received_in_batch;
        {
          std::lock_guard lock(mutex_);
          ++stats_.received_udp_packets;
        }
        quiche_recv_info receive_info = {
            .from = reinterpret_cast<sockaddr*>(&peer_address),
            .from_len = peer_length,
            .to = reinterpret_cast<sockaddr*>(&local_address.value),
            .to_len = local_address.length,
        };
        const ssize_t processed =
            quiche_conn_recv(connection.get(), packet.data(),
                             static_cast<size_t>(received), &receive_info);
        if (processed < 0 && processed != QUICHE_ERR_DONE) {
          Fail("quiche receive failed: " + std::to_string(processed));
          return;
        }
        if (!DrainReceived(connection.get()) ||
            !DrainReliableMessages(connection.get())) {
          Fail("failed to receive RoQ data");
          return;
        }
      }

      DispatchReceived();
      if ((quiche_conn_is_established(connection.get()) ||
           quiche_conn_is_in_early_data(connection.get())) &&
          !SendReliableMessages(connection.get())) {
        Fail("failed to queue reliable message");
        return;
      }
      if (connection && quiche_conn_is_established(connection.get())) {
        bool became_established = false;
        {
          std::lock_guard lock(mutex_);
          if (!stats_.established) {
            became_established = true;
            stats_.established = true;
            stats_.session_resumed = quiche_conn_is_resumed(connection.get());
            stats_.early_data_reason =
                quiche_conn_early_data_reason(connection.get());
            stats_.early_data_accepted =
                stats_.early_data_attempted &&
                stats_.early_data_reason == QUICHE_EARLY_DATA_ACCEPTED;
            stats_.early_data_fallback =
                stats_.early_data_attempted && !stats_.early_data_accepted;
            if (stats_.early_data_fallback) {
              stats_.early_rejection_queue_drops +=
                  outgoing_.Clear() + stats_.early_datagrams_sent;
              quiche_conn_dgram_purge_outgoing(connection.get(), PurgeDatagram);
              CopyQueueStatsLocked();
            }
            condition_.notify_all();
          }
        }
        if (became_established)
          NotifyLifecycle(RoqSessionLifecycleState::kEstablished);
        UpdateDatagramLimit(connection.get());
        UpdatePathStats(connection.get());
        bool request_ping = false;
        {
          std::lock_guard lock(mutex_);
          request_ping = ping_requested_;
          ping_requested_ = false;
        }
        if (request_ping &&
            quiche_conn_send_ack_eliciting(connection.get()) < 0) {
          Fail("failed to send QUIC PING");
          return;
        }
        if (cache) {
          std::array<uint8_t, 1024> token = {};
          while (true) {
            const ssize_t token_length = quiche_conn_recv_new_token(
                connection.get(), token.data(), token.size());
            if (token_length == QUICHE_ERR_DONE)
              break;
            if (token_length < 0) {
              Fail("failed to receive address validation token");
              return;
            }
            cached_address_token.assign(token.begin(),
                                        token.begin() + token_length);
            cache_dirty_ = true;
            {
              std::lock_guard lock(mutex_);
              ++stats_.address_tokens_received;
            }
          }
          const uint8_t* session_data = nullptr;
          size_t session_length = 0;
          quiche_conn_session(connection.get(), &session_data, &session_length);
          if (session_data && session_length > 0 &&
              !stats().early_data_fallback &&
              (!session_cached_ || cache_dirty_)) {
            std::string cache_error;
            const Timestamp cache_now =
                config_.resumption_clock ? config_.resumption_clock()
                                         : Timestamp::Micros(TimeUTCMicros());
            if (!cache->Save(cache_key,
                             {.created_at = cache_now,
                              .session =
                                  [&] {
                                    const std::span<const uint8_t> session(
                                        session_data, session_length);
                                    return std::vector<uint8_t>(session.begin(),
                                                                session.end());
                                  }(),
                              .address_token = cached_address_token},
                             &cache_error)) {
              RTC_LOG(LS_WARNING) << cache_error;
            } else {
              session_cached_ = true;
              cache_dirty_ = false;
              std::lock_guard lock(mutex_);
              stats_.resumption_cache_saved = true;
            }
          }
        }
      }

      bool request_close = false;
      uint64_t close_code = 0;
      std::string close_reason;
      {
        std::lock_guard lock(mutex_);
        request_close = close_requested_;
        close_code = close_code_;
        close_reason = close_reason_;
      }
      if (!connection && request_close)
        break;
      if (connection && request_close && !local_close_sent) {
        quiche_conn_close(connection.get(), true, close_code,
                          reinterpret_cast<const uint8_t*>(close_reason.data()),
                          close_reason.size());
        local_close_sent = true;
      }

      if (connection) {
        TrimVideoQueue(connection.get());
        ObserveSendQueue(connection.get());
        const uint64_t timeout = quiche_conn_timeout_as_nanos(connection.get());
        if (timeout != kNoTimeout && timeout == 0)
          quiche_conn_on_timeout(connection.get());
      }
      if (pending_udp && !socket_write_blocked &&
          SteadyClock::now() >= pending_udp->send_at) {
        PendingUdpPacket pending = std::move(*pending_udp);
        pending_udp.reset();
        if (!send_or_defer_udp(std::move(pending)))
          return;
      }
      if (connection && !pending_udp) {
        {
          std::lock_guard lock(mutex_);
          quiche_conn_set_bandwidth_probe_max_rate(connection.get(),
                                                   probe_max_bytes_per_second_);
          quiche_conn_set_bandwidth_maintenance_max_rate(
              connection.get(), maintenance_max_bytes_per_second_);
          quiche_conn_set_external_dgram_queue_bytes(connection.get(),
                                                     outgoing_.byte_size());
        }
        ObserveSendQueue(connection.get());
        quiche_send_info send_info = {};
        const ssize_t written =
            quiche_conn_send(connection.get(), packet.data(),
                             config_.maximum_udp_payload, &send_info);
        if (written >= 0) {
          send_progress = true;
          PendingUdpPacket pending;
          {
            std::lock_guard lock(mutex_);
            if (quiche_slot_datagram_ &&
                quiche_conn_dgram_send_queue_len(connection.get()) == 0) {
              pending.datagram = std::move(quiche_slot_datagram_);
              quiche_slot_datagram_.reset();
            }
          }
          pending.bytes.assign(packet.begin(),
                               packet.begin() + static_cast<size_t>(written));
          pending.destination = send_info.to;
          pending.destination_length = send_info.to_len;
          pending.send_at =
              SteadyClock::now() +
              std::chrono::nanoseconds(send_info.pacing_delay_nanos);
          {
            std::lock_guard lock(mutex_);
            stats_.max_pacing_delay_ns = std::max(stats_.max_pacing_delay_ns,
                                                  send_info.pacing_delay_nanos);
          }
          if (!send_or_defer_udp(std::move(pending)))
            return;
        } else if (written != QUICHE_ERR_DONE) {
          Fail("quiche send failed: " + std::to_string(written));
          return;
        } else if (local_close_sent) {
          close_drained = true;
        }
      }

      bool can_send_datagram = false;
      if (connection) {
        std::lock_guard lock(mutex_);
        can_send_datagram = quiche_conn_is_established(connection.get()) ||
                            (early_datagrams_allowed_ &&
                             quiche_conn_is_in_early_data(connection.get()));
      }
      for (size_t attempted = 0; can_send_datagram; ++attempted) {
        TrimVideoQueue(connection.get());
        const ssize_t queue_length =
            quiche_conn_dgram_send_queue_len(connection.get());
        bool purge_for_pmtu = false;
        bool purge_for_binding = false;
        size_t pending_media = 0;
        {
          std::lock_guard lock(mutex_);
          stats_.peak_quiche_queue_size =
              std::max(stats_.peak_quiche_queue_size,
                       static_cast<size_t>(std::max<ssize_t>(queue_length, 0)));
          purge_for_pmtu = purge_quiche_slot_for_pmtu_;
          purge_for_binding = purge_quiche_slot_for_binding_;
          pending_media = outgoing_.size();
        }
        if (queue_length == 0) {
          std::lock_guard lock(mutex_);
          quiche_slot_datagram_.reset();
          purge_quiche_slot_for_pmtu_ = false;
          purge_quiche_slot_for_binding_ = false;
        } else if (purge_for_pmtu || purge_for_binding) {
          quiche_conn_dgram_purge_outgoing(connection.get(), PurgeDatagram);
          std::lock_guard lock(mutex_);
          if (purge_for_pmtu)
            ++quiche_pmtu_purge_drops_;
          if (!purge_for_binding && quiche_slot_datagram_) {
            outgoing_.DiscardFrame(
                *quiche_slot_datagram_,
                Timestamp::Micros(config_.clock->CurrentTime().us()));
          }
          quiche_slot_datagram_.reset();
          purge_quiche_slot_for_pmtu_ = false;
          purge_quiche_slot_for_binding_ = false;
          CopyQueueStatsLocked();
          continue;
        }
        if (!ShouldContinueDatagramBurst(
                attempted, kMediaBurstLimit, pending_udp.has_value(),
                static_cast<size_t>(std::max<ssize_t>(queue_length, 0)),
                pending_media)) {
          break;
        }
        if (queue_length != 0) {
          {
            std::lock_guard lock(mutex_);
            quiche_conn_set_bandwidth_probe_max_rate(
                connection.get(), probe_max_bytes_per_second_);
            quiche_conn_set_bandwidth_maintenance_max_rate(
                connection.get(), maintenance_max_bytes_per_second_);
            quiche_conn_set_external_dgram_queue_bytes(connection.get(),
                                                       outgoing_.byte_size());
          }
          ObserveSendQueue(connection.get());
          quiche_send_info send_info = {};
          const ssize_t written =
              quiche_conn_send(connection.get(), packet.data(),
                               config_.maximum_udp_payload, &send_info);
          if (written == QUICHE_ERR_DONE)
            break;
          if (written < 0) {
            Fail("quiche send failed: " + std::to_string(written));
            return;
          }
          send_progress = true;
          PendingUdpPacket pending;
          {
            std::lock_guard lock(mutex_);
            if (quiche_slot_datagram_ &&
                quiche_conn_dgram_send_queue_len(connection.get()) == 0) {
              pending.datagram = std::move(quiche_slot_datagram_);
              quiche_slot_datagram_.reset();
            }
          }
          pending.bytes.assign(packet.begin(),
                               packet.begin() + static_cast<size_t>(written));
          pending.destination = send_info.to;
          pending.destination_length = send_info.to_len;
          pending.send_at =
              SteadyClock::now() +
              std::chrono::nanoseconds(send_info.pacing_delay_nanos);
          {
            std::lock_guard lock(mutex_);
            stats_.max_pacing_delay_ns = std::max(stats_.max_pacing_delay_ns,
                                                  send_info.pacing_delay_nanos);
          }
          if (!send_or_defer_udp(std::move(pending)))
            return;
          continue;
        }
        std::unique_lock lock(mutex_);
        auto outgoing = outgoing_.Pop(config_.clock->CurrentTime());
        if (!outgoing)
          break;
        const ssize_t queued = quiche_conn_dgram_send(
            connection.get(), outgoing->bytes.data(), outgoing->bytes.size());
        if (queued == static_cast<ssize_t>(outgoing->bytes.size())) {
          send_progress = true;
          quiche_slot_datagram_ = std::move(*outgoing);
        } else if (queued == QUICHE_ERR_BUFFER_TOO_SHORT) {
          ++quiche_oversize_drops_;
          outgoing_.DiscardFrame(*outgoing, config_.clock->CurrentTime());
          CopyQueueStatsLocked();
        } else if (queued == QUICHE_ERR_DONE) {
          outgoing_.Requeue(std::move(*outgoing));
          break;
        } else {
          error_ = "failed to queue RoQ DATAGRAM: " + std::to_string(queued);
          close_requested_ = true;
          close_code_ = static_cast<uint64_t>(RoqError::kInternalError);
          close_reason_ = error_;
          // The failure created immediate close work on this thread. Do not
          // park until a remote event or a transport timeout to apply it.
          send_progress = true;
          break;
        }
      }

      if (connection && quiche_conn_is_closed(connection.get()) &&
          !pending_udp) {
        break;
      }
      if (local_close_sent && close_drained && !pending_udp)
        break;
      // Continue productive batches, but do not spin just because media is
      // queued: pacing and cwnd may require a deadline or an incoming ACK.
      if (received_in_batch == kReceiveBurstLimit ||
          (send_progress && !pending_udp))
        continue;

      RoqSocketWaiter::Deadline deadline;
      const auto wait_now = SteadyClock::now();
      const uint64_t timeout_ns =
          quiche_conn_timeout_as_nanos(connection.get());
      if (timeout_ns != kNoTimeout) {
        const auto maximum =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                SteadyClock::time_point::max() - wait_now)
                .count();
        deadline = wait_now + std::chrono::nanoseconds(std::min(
                                  timeout_ns, static_cast<uint64_t>(maximum)));
      }
      if (pending_udp && !socket_write_blocked &&
          (!deadline || pending_udp->send_at < *deadline))
        deadline = pending_udp->send_at;
      {
        std::lock_guard lock(mutex_);
        const auto expires = std::min(
            outgoing_.NextVideoExpiry(
                quiche_slot_datagram_ ? &*quiche_slot_datagram_ : nullptr),
            outgoing_.NextRecoveryService(config_.clock->CurrentTime()));
        if (expires.IsFinite()) {
          const auto delay = std::max(TimeDelta::Zero(),
                                      expires - config_.clock->CurrentTime());
          const auto age_deadline =
              wait_now + std::chrono::microseconds(delay.us());
          if (!deadline || age_deadline < *deadline)
            deadline = age_deadline;
        }
      }
      const auto events = waiter_.Wait(deadline, socket_write_blocked);
      if (events.error) {
        Fail("network event wait failed: " + std::to_string(events.error));
        return;
      }
      if (events.writable)
        socket_write_blocked = false;
    }

    if (connection)
      UpdatePathStats(connection.get());
    {
      std::lock_guard lock(mutex_);
      CopyQueueStatsLocked();
      stats_.clean_shutdown = error_.empty();
      outgoing_.Clear();
      quiche_slot_datagram_.reset();
      closed_ = true;
      condition_.notify_all();
    }
    NotifyLifecycle(RoqSessionLifecycleState::kClosed);
  }

  RoqSocketWaiter waiter_;
  const RoqSessionConfig config_;
  const ReceiveCallback receive_callback_;
  const DatagramSizeCallback datagram_size_callback_;
  const ReliableMessageCallback reliable_message_callback_;
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  BoundedDatagramQueue outgoing_;
  ReceiveFlowRegistry receive_flows_;
  std::vector<uint64_t> send_flow_ids_;
  std::deque<std::vector<uint8_t>> reliable_messages_;
  size_t reliable_message_offset_ = 0;
  ReliableMessageDecoder reliable_decoder_;
  std::thread thread_;
  bool ready_ = false;
  bool close_requested_ = false;
  bool closed_ = false;
  uint64_t close_code_ = 0;
  std::string close_reason_ = "complete";
  std::optional<size_t> application_datagram_limit_;
  std::optional<QueuedDatagram> quiche_slot_datagram_;
  bool purge_quiche_slot_for_pmtu_ = false;
  bool purge_quiche_slot_for_binding_ = false;
  size_t quiche_pmtu_purge_drops_ = 0;
  size_t quiche_oversize_drops_ = 0;
  bool session_cached_ = false;
  bool cache_dirty_ = false;
  bool early_datagrams_allowed_ = config_.allow_early_datagrams;
  uint64_t probe_max_bytes_per_second_ = 0;
  uint64_t maintenance_max_bytes_per_second_ = 0;
  bool ping_requested_ = false;
  bool abort_requested_ = false;
  std::deque<RoqReceivedPacket> pending_received_;
  std::map<uint64_t, uint64_t> receive_generations_;
  size_t pending_received_bytes_ = 0;
  RoqSessionStats stats_;
  std::string error_;
  uint64_t last_path_acked_bytes_ = 0;
  const bool media_diagnostics_ =
      std::getenv("ARTC_MEDIA_DIAGNOSTICS") != nullptr;
  Timestamp last_diagnostic_at_ = Timestamp::MinusInfinity();
};

}  // namespace

RoqUdpSendDisposition ClassifyUdpSendResult(int64_t result,
                                            size_t expected_bytes,
                                            int error) {
  if (result == static_cast<int64_t>(expected_bytes))
    return RoqUdpSendDisposition::kSent;
  if (result < 0 && (error == EAGAIN || error == EWOULDBLOCK ||
                     error == ENOBUFS || error == EINTR)) {
    return RoqUdpSendDisposition::kRetry;
  }
  return RoqUdpSendDisposition::kFatal;
}

std::unique_ptr<RoqSession> RoqSession::Create(
    RoqSessionConfig config,
    ReceiveCallback receive_callback,
    DatagramSizeCallback datagram_size_callback,
    ReliableMessageCallback reliable_message_callback) {
  return std::make_unique<RoqSessionImpl>(
      std::move(config), std::move(receive_callback),
      std::move(datagram_size_callback), std::move(reliable_message_callback));
}

}  // namespace webrtc::roq
