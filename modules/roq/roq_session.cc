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
#include "modules/roq/roq_wire.h"
#include "quiche.h"
#include "rtc_base/crypto_random.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"

namespace webrtc::roq {
namespace {

using SteadyClock = std::chrono::steady_clock;
constexpr size_t kConnectionIdLength = 16;
constexpr size_t kPacketBufferSize = 65535;
constexpr uint64_t kControlStreamId = 0;
constexpr size_t kEarlyDatagramPayloadLimit = 1000;
constexpr uint64_t kNoTimeout = std::numeric_limits<uint64_t>::max();

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
        outgoing_(config_.outgoing_queue_capacity, config_.maximum_udp_payload),
        receive_flows_(config_.receive_flow_ids.empty()
                           ? config_.allowed_flow_ids
                           : config_.receive_flow_ids),
        send_flow_ids_(config_.allowed_flow_ids) {}

  ~RoqSessionImpl() override {
    Close(0, "complete");
    if (thread_.joinable())
      thread_.join();
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
    return true;
  }

  void BeginReceiveFlowUpdate() override {
    std::lock_guard lock(mutex_);
    receive_flows_.BeginUpdate(config_.clock->CurrentTime());
  }

  void UpdateReceiveFlowIds(std::vector<uint64_t> flow_ids) override {
    std::vector<RoqReceivedPacket> released;
    ReceiveCallback callback;
    {
      std::lock_guard lock(mutex_);
      released = receive_flows_.Update(std::move(flow_ids));
      CopyFlowStatsLocked();
      callback = receive_callback_;
    }
    if (callback) {
      for (RoqReceivedPacket& packet : released)
        callback(std::move(packet));
    }
  }

  void BeginFlowUpdate() override { BeginReceiveFlowUpdate(); }

  void CommitFlowUpdate(std::vector<RoqFlowConfig> flows) override {
    std::vector<ReceiveFlowConfig> receive_flows;
    std::vector<uint64_t> send_flows;
    receive_flows.reserve(flows.size());
    for (const RoqFlowConfig& flow : flows) {
      receive_flows.push_back(
          {flow.flow_id,
           flow.receive_policy == RoqReceiveFlowPolicy::kDeliver
               ? ReceiveFlowPolicy::kDeliver
               : ReceiveFlowPolicy::kDiscard});
      if (flow.allow_send)
        send_flows.push_back(flow.flow_id);
    }
    std::vector<RoqReceivedPacket> released;
    ReceiveCallback callback;
    {
      std::lock_guard lock(mutex_);
      released = receive_flows_.Update(std::move(receive_flows));
      send_flow_ids_ = std::move(send_flows);
      CopyFlowStatsLocked();
      callback = receive_callback_;
    }
    if (callback) {
      for (RoqReceivedPacket& packet : released)
        callback(std::move(packet));
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
        .flow_id = flow_id,
        .media_type = metadata.media_type,
        .enqueue_time = metadata.enqueue_time,
        .deadline = metadata.deadline,
        .priority = metadata.priority,
        .frame_id = metadata.frame_id,
        .packet_id = metadata.packet_id,
        .original_packet_id = metadata.original_packet_id,
        .on_submitted = std::move(on_submitted),
    };
    const DatagramEnqueueResult result =
        outgoing_.Enqueue(std::move(datagram), config_.clock->CurrentTime());
    if (result == DatagramEnqueueResult::kQueued && metadata.frame_id &&
        quiche_slot_frame_id_ == metadata.frame_id) {
      const std::optional<Timestamp> frame_deadline =
          outgoing_.FrameDeadline(*metadata.frame_id);
      if (frame_deadline)
        quiche_slot_deadline_ = *frame_deadline;
    }
    CopyQueueStatsLocked();
    switch (result) {
      case DatagramEnqueueResult::kQueued:
        return RoqSendResult::kQueued;
      case DatagramEnqueueResult::kQueueFull:
        return RoqSendResult::kQueueFull;
      case DatagramEnqueueResult::kOversize:
        return RoqSendResult::kOversize;
      case DatagramEnqueueResult::kInvalid:
        return RoqSendResult::kInvalid;
    }
  }

  void RecordInvalidDatagram() override {
    std::lock_guard lock(mutex_);
    outgoing_.RecordInvalid();
    CopyQueueStatsLocked();
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
      outgoing_.SetMaxDatagramSize(effective);
      if (quiche_slot_size_ > effective)
        purge_quiche_slot_for_pmtu_ = true;
      CopyQueueStatsLocked();
      callback = datagram_size_callback_;
      packet_limit = effective > 0 ? effective - 1 : 0;
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

  std::string error() const override {
    std::lock_guard lock(mutex_);
    return error_;
  }

 private:
  void Fail(std::string error) {
    std::lock_guard lock(mutex_);
    if (error_.empty())
      error_ = std::move(error);
    closed_ = true;
    condition_.notify_all();
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
        quiche_config_set_cc_algorithm_name(config.get(), "bbr2_gcongestion") !=
            0) {
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

  void CopyQueueStatsLocked() {
    stats_.queue_full_drops = outgoing_.queue_full_drops();
    stats_.oversize_drops = outgoing_.oversize_drops() + quiche_oversize_drops_;
    stats_.pmtu_purge_drops =
        outgoing_.pmtu_purge_drops() + quiche_pmtu_purge_drops_;
    stats_.expired_drops = outgoing_.expired_drops();
    stats_.invalid_drops = outgoing_.invalid_drops();
    stats_.peak_queue_size = outgoing_.peak_size();
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
    std::lock_guard lock(mutex_);
    stats_.rtt_ns = path.rtt;
    stats_.max_rtt_ns = path.max_rtt;
    stats_.delivery_rate_bytes_per_second = path.delivery_rate;
    stats_.max_bandwidth_bytes_per_second = path.max_bandwidth;
    stats_.acked_bytes = connection_stats.acked_bytes;
    stats_.startup_exit_cwnd = path.startup_exit_cwnd;
    stats_.pmtu_bytes = path.pmtu;
    stats_.dgram_lost = path.dgram_lost;
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
      outgoing_.SetMaxDatagramSize(effective);
      if (quiche_slot_size_ > effective)
        purge_quiche_slot_for_pmtu_ = true;
      CopyQueueStatsLocked();
      callback = datagram_size_callback_;
      packet_limit = effective - 1;
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
          std::vector<uint8_t>(decoded.packet.begin(), decoded.packet.end())};
      ReceiveFlowRoute route;
      {
        std::lock_guard lock(mutex_);
        route = receive_flows_.Route(received, config_.clock->CurrentTime());
        CopyFlowStatsLocked();
      }
      if (route == ReceiveFlowRoute::kClose) {
        constexpr std::string_view reason = "unknown RoQ flow ID";
        quiche_conn_close(
            connection, true, static_cast<uint64_t>(RoqError::kUnknownFlowId),
            reinterpret_cast<const uint8_t*>(reason.data()), reason.size());
        return true;
      }
      if (route == ReceiveFlowRoute::kBuffered)
        continue;
      ReceiveCallback callback;
      {
        std::lock_guard lock(mutex_);
        ++stats_.received_datagrams;
        if (!stats_.established)
          ++stats_.early_datagrams_received;
        callback = receive_callback_;
      }
      if (callback) {
        callback(std::move(received));
      }
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
        outgoing_.SetMaxDatagramSize(kEarlyDatagramPayloadLimit);
      }
    }
    receive_flows_.BeginUpdate(config_.clock->CurrentTime());
    {
      std::lock_guard lock(mutex_);
      ready_ = true;
      condition_.notify_all();
    }

    std::optional<PendingUdpPacket> pending_udp;
    bool local_close_sent = false;
    bool close_drained = false;
    std::array<uint8_t, kPacketBufferSize> packet = {};
    while (true) {
      while (true) {
        sockaddr_storage peer_address = {};
        socklen_t peer_length = sizeof(peer_address);
        const ssize_t received =
            recvfrom(socket_fd.get(), packet.data(), packet.size(), 0,
                     reinterpret_cast<sockaddr*>(&peer_address), &peer_length);
        if (received < 0) {
          if (errno != EAGAIN && errno != EWOULDBLOCK) {
            Fail("UDP receive failed");
            return;
          }
          break;
        }
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

      const auto now = SteadyClock::now();
      if ((quiche_conn_is_established(connection.get()) ||
           quiche_conn_is_in_early_data(connection.get())) &&
          !SendReliableMessages(connection.get())) {
        Fail("failed to queue reliable message");
        return;
      }
      if (connection && quiche_conn_is_established(connection.get())) {
        bool first_established = false;
        {
          std::lock_guard lock(mutex_);
          if (!stats_.established) {
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
            first_established = true;
            condition_.notify_all();
          }
        }
        if (first_established)
          UpdateDatagramLimit(connection.get());
        UpdateDatagramLimit(connection.get());
        UpdatePathStats(connection.get());
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

      if (connection && (quiche_conn_is_established(connection.get()) ||
                         quiche_conn_is_in_early_data(connection.get()))) {
        std::optional<QueuedDatagram> outgoing;
        {
          std::lock_guard lock(mutex_);
          if (quiche_conn_dgram_send_queue_len(connection.get()) == 0)
            outgoing = outgoing_.Pop(
                Timestamp::Micros(config_.clock->CurrentTime().us()));
        }
        if (outgoing) {
          const ssize_t queued = quiche_conn_dgram_send(
              connection.get(), outgoing->bytes.data(), outgoing->bytes.size());
          if (queued == static_cast<ssize_t>(outgoing->bytes.size())) {
            {
              std::lock_guard lock(mutex_);
              outgoing_.RecordSubmitted(*outgoing);
              ++stats_.sent_datagrams;
              if (!stats_.established) {
                ++stats_.early_datagrams_sent;
                stats_.early_data_attempted = true;
              }
              quiche_slot_deadline_ = outgoing->deadline;
              quiche_slot_size_ = outgoing->bytes.size();
              quiche_slot_frame_id_ = outgoing->frame_id;
            }
            if (outgoing->on_submitted)
              outgoing->on_submitted();
          } else if (queued == QUICHE_ERR_BUFFER_TOO_SHORT) {
            std::lock_guard lock(mutex_);
            ++quiche_oversize_drops_;
            CopyQueueStatsLocked();
          } else if (queued != QUICHE_ERR_DONE) {
            std::lock_guard lock(mutex_);
            error_ = "failed to queue RoQ DATAGRAM: " + std::to_string(queued);
            close_requested_ = true;
            close_code_ = static_cast<uint64_t>(RoqError::kInternalError);
            close_reason_ = error_;
          } else {
            std::lock_guard lock(mutex_);
            outgoing_.Enqueue(
                std::move(*outgoing),
                Timestamp::Micros(config_.clock->CurrentTime().us()));
          }
        }
      }
      if (connection) {
        const ssize_t queue_length =
            quiche_conn_dgram_send_queue_len(connection.get());
        bool purge_for_expiry = false;
        bool purge_for_pmtu = false;
        {
          std::lock_guard lock(mutex_);
          purge_for_expiry =
              quiche_slot_deadline_.has_value() &&
              *quiche_slot_deadline_ <= config_.clock->CurrentTime();
          purge_for_pmtu = purge_quiche_slot_for_pmtu_;
        }
        if (queue_length == 0) {
          std::lock_guard lock(mutex_);
          quiche_slot_deadline_.reset();
          quiche_slot_size_ = 0;
          quiche_slot_frame_id_.reset();
          purge_quiche_slot_for_pmtu_ = false;
        } else if (purge_for_expiry || purge_for_pmtu) {
          quiche_conn_dgram_purge_outgoing(connection.get(), PurgeDatagram);
          std::lock_guard lock(mutex_);
          if (purge_for_expiry)
            ++stats_.quiche_slot_expired_drops;
          if (purge_for_pmtu)
            ++quiche_pmtu_purge_drops_;
          quiche_slot_deadline_.reset();
          quiche_slot_size_ = 0;
          quiche_slot_frame_id_.reset();
          purge_quiche_slot_for_pmtu_ = false;
          CopyQueueStatsLocked();
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
        const uint64_t timeout = quiche_conn_timeout_as_nanos(connection.get());
        if (timeout != kNoTimeout && timeout == 0)
          quiche_conn_on_timeout(connection.get());
      }
      if (pending_udp && now >= pending_udp->send_at) {
        const ssize_t sent =
            sendto(socket_fd.get(), pending_udp->bytes.data(),
                   pending_udp->bytes.size(), 0,
                   reinterpret_cast<const sockaddr*>(&pending_udp->destination),
                   pending_udp->destination_length);
        if (sent != static_cast<ssize_t>(pending_udp->bytes.size())) {
          Fail("UDP send failed");
          return;
        }
        {
          std::lock_guard lock(mutex_);
          ++stats_.sent_udp_packets;
        }
        pending_udp.reset();
      }
      if (connection && !pending_udp) {
        quiche_send_info send_info = {};
        const ssize_t written =
            quiche_conn_send(connection.get(), packet.data(),
                             config_.maximum_udp_payload, &send_info);
        if (written >= 0) {
          PendingUdpPacket pending;
          pending.bytes.assign(packet.begin(),
                               packet.begin() + static_cast<size_t>(written));
          pending.destination = send_info.to;
          pending.destination_length = send_info.to_len;
          pending.send_at =
              now + std::chrono::nanoseconds(send_info.pacing_delay_nanos);
          {
            std::lock_guard lock(mutex_);
            stats_.max_pacing_delay_ns = std::max(stats_.max_pacing_delay_ns,
                                                  send_info.pacing_delay_nanos);
          }
          pending_udp = std::move(pending);
        } else if (written != QUICHE_ERR_DONE) {
          Fail("quiche send failed: " + std::to_string(written));
          return;
        } else if (local_close_sent) {
          close_drained = true;
        }
      }

      if (connection && quiche_conn_is_closed(connection.get()) &&
          !pending_udp) {
        break;
      }
      if (local_close_sent && close_drained && !pending_udp)
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (connection)
      UpdatePathStats(connection.get());
    {
      std::lock_guard lock(mutex_);
      CopyQueueStatsLocked();
      stats_.clean_shutdown = error_.empty();
      closed_ = true;
      condition_.notify_all();
    }
  }

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
  std::optional<Timestamp> quiche_slot_deadline_;
  size_t quiche_slot_size_ = 0;
  std::optional<RoqFrameId> quiche_slot_frame_id_;
  bool purge_quiche_slot_for_pmtu_ = false;
  size_t quiche_pmtu_purge_drops_ = 0;
  size_t quiche_oversize_drops_ = 0;
  bool session_cached_ = false;
  bool cache_dirty_ = false;
  RoqSessionStats stats_;
  std::string error_;
};

}  // namespace

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
