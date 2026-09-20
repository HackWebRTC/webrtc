/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/roq/network_lab.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace webrtc::roq {
namespace {

using SteadyClock = std::chrono::steady_clock;
constexpr size_t kPacketSize = 64;
constexpr size_t kMaximumPacketSize = 65535;
constexpr int64_t kBaseDelayUs = 50000;
constexpr int64_t kJitterUs = 10000;
constexpr int64_t kReorderDelayUs = 20000;
constexpr int64_t kQueueLimitUs = 200000;
constexpr uint64_t kBitsPerSecond = 10000000;

struct Socket {
  explicit Socket(int value = -1) : value(value) {}
  ~Socket() {
    if (value >= 0) {
      close(value);
    }
  }
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;
  Socket(Socket&& other) noexcept : value(std::exchange(other.value, -1)) {}
  Socket& operator=(Socket&& other) noexcept {
    if (this != &other) {
      if (value >= 0) {
        close(value);
      }
      value = std::exchange(other.value, -1);
    }
    return *this;
  }
  int value;
};

struct ScheduledPacket {
  SteadyClock::time_point delivery_time;
  std::vector<uint8_t> bytes;
  uint32_t sequence = 0;
};

struct EarliestPacketFirst {
  bool operator()(const ScheduledPacket& left,
                  const ScheduledPacket& right) const {
    return left.delivery_time > right.delivery_time;
  }
};

class DeterministicRandom {
 public:
  explicit DeterministicRandom(uint64_t seed) : state_(seed) {}

  uint64_t Next() {
    state_ ^= state_ << 13;
    state_ ^= state_ >> 7;
    state_ ^= state_ << 17;
    return state_;
  }

  bool Percent(uint64_t hundredths) { return Next() % 10000 < hundredths; }

  int64_t Jitter(int64_t range_us = kJitterUs) {
    return static_cast<int64_t>(Next() % (2 * range_us + 1)) - range_us;
  }

 private:
  uint64_t state_;
};

class EventLog {
 public:
  EventLog(const std::string& path, std::string run_id)
      : output_(path), run_id_(std::move(run_id)) {}

  bool ok() const { return output_.good(); }

  void Write(const char* event, const char* direction, uint32_t sequence) {
    const auto now = SteadyClock::now();
    const int64_t monotonic_us =
        std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch())
            .count();
    const int64_t utc_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    output_ << "{\"schema_version\":1,\"run_id\":\"" << run_id_
            << "\",\"process\":\"udp-relay\",\"event\":\"" << event
            << "\",\"direction\":\"" << direction
            << "\",\"sequence\":" << next_event_sequence_++
            << ",\"packet_sequence\":" << sequence
            << ",\"monotonic_us\":" << monotonic_us << ",\"utc_ms\":" << utc_ms
            << "}\n";
  }

 private:
  std::ofstream output_;
  std::string run_id_;
  uint64_t next_event_sequence_ = 0;
};

Socket BoundSocket(sockaddr_in* address, bool nonblocking, std::string* error) {
  Socket socket_fd(socket(AF_INET, SOCK_DGRAM, 0));
  if (socket_fd.value < 0) {
    *error = "failed to create UDP socket";
    return socket_fd;
  }
  *address = {};
  address->sin_family = AF_INET;
  address->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(socket_fd.value, reinterpret_cast<const sockaddr*>(address),
           sizeof(*address)) != 0) {
    *error = "failed to bind UDP socket";
    return Socket();
  }
  socklen_t address_length = sizeof(*address);
  if (getsockname(socket_fd.value, reinterpret_cast<sockaddr*>(address),
                  &address_length) != 0) {
    *error = "failed to read UDP socket address";
    return Socket();
  }
  if (nonblocking && fcntl(socket_fd.value, F_SETFL, O_NONBLOCK) != 0) {
    *error = "failed to make UDP socket nonblocking";
    return Socket();
  }
  timeval timeout = {.tv_sec = 2, .tv_usec = 0};
  setsockopt(socket_fd.value, SOL_SOCKET, SO_RCVTIMEO, &timeout,
             sizeof(timeout));
  return socket_fd;
}

std::array<uint8_t, kPacketSize> MakePacket(uint32_t sequence) {
  std::array<uint8_t, kPacketSize> packet = {};
  const uint32_t network_sequence = htonl(sequence);
  std::memcpy(packet.data(), &network_sequence, sizeof(network_sequence));
  for (size_t index = sizeof(network_sequence); index < packet.size();
       ++index) {
    packet[index] = static_cast<uint8_t>((sequence + index) & 0xff);
  }
  return packet;
}

uint32_t PacketSequence(const std::array<uint8_t, kPacketSize>& packet) {
  uint32_t network_sequence = 0;
  std::memcpy(&network_sequence, packet.data(), sizeof(network_sequence));
  return ntohl(network_sequence);
}

bool SendPacket(int socket_fd,
                const sockaddr_in& destination,
                const std::array<uint8_t, kPacketSize>& packet,
                size_t size) {
  return sendto(socket_fd, packet.data(), size, 0,
                reinterpret_cast<const sockaddr*>(&destination),
                sizeof(destination)) == static_cast<ssize_t>(size);
}

bool ReceivePacket(int socket_fd,
                   std::array<uint8_t, kPacketSize>* packet,
                   size_t* size) {
  const ssize_t received =
      recvfrom(socket_fd, packet->data(), packet->size(), 0, nullptr, nullptr);
  if (received < 0) {
    return false;
  }
  *size = static_cast<size_t>(received);
  return true;
}

class ImpairmentDirection {
 public:
  ImpairmentDirection(uint64_t seed,
                      const char* name,
                      EventLog* event_log,
                      UdpSelfTestResult* result,
                      std::string profile = "m0")
      : seed_(seed),
        name_(name),
        event_log_(event_log),
        result_(result),
        profile_(std::move(profile)),
        profile_start_(SteadyClock::now()) {}

  void Ingest(const uint8_t* bytes, size_t size, uint32_t sequence) {
    event_log_->Write("relay_ingress", name_, sequence);
    DeterministicRandom random(seed_ ^ ((static_cast<uint64_t>(sequence) + 1) *
                                        0x9e3779b97f4a7c15ULL));
    const auto now = SteadyClock::now();
    const auto profile_elapsed = now - profile_start_;
    // A short pre-congestion burst loss exercises both media hops even when
    // early bandwidth adaptation produces video packets smaller than audio.
    if (profile_ == "m4" &&
        profile_elapsed >= std::chrono::seconds(2) &&
        profile_elapsed < std::chrono::milliseconds(2250)) {
      ++result_->injected_loss;
      event_log_->Write("injected_loss", name_, sequence);
      return;
    }
    if (profile_ == "m5" && profile_elapsed >= std::chrono::seconds(5) &&
        profile_elapsed < std::chrono::seconds(11)) {
      ++result_->injected_loss;
      event_log_->Write("injected_outage", name_, sequence);
      return;
    }
    const bool constrained_m4 = IsM4Constrained(now);
    const uint64_t loss_hundredths = constrained_m4 ? 200 : 100;
    if (profile_ != "m3" && profile_ != "m5" &&
        (profile_ != "m4" || constrained_m4) &&
        random.Percent(loss_hundredths)) {
      ++result_->injected_loss;
      event_log_->Write("injected_loss", name_, sequence);
      return;
    }
    const bool reordered = profile_ != "m3" && profile_ != "m5" &&
                           (profile_ != "m4" || constrained_m4) &&
                           random.Percent(constrained_m4 ? 500 : 100);
    if (reordered) {
      ++result_->injected_reorder;
    }
    const uint64_t bandwidth_bps = BandwidthBps(now);
    const int64_t serialization_us = static_cast<int64_t>(
        (size * 8 * 1000000ULL + bandwidth_bps - 1) / bandwidth_bps);
    const auto serialization_start = std::max(now, next_serialization_);
    next_serialization_ =
        serialization_start + std::chrono::microseconds(serialization_us);
    if (next_serialization_ - now > std::chrono::microseconds(kQueueLimitUs)) {
      ++result_->injected_loss;
      event_log_->Write("queue_drop", name_, sequence);
      return;
    }
    const int64_t jitter_us =
        profile_ == "m3" || profile_ == "m5" ||
                (profile_ == "m4" && !constrained_m4)
            ? 0
            : random.Jitter(constrained_m4 ? 20000 : kJitterUs);
    const int64_t base_delay_us = profile_ == "m4"
                                      ? (constrained_m4 ? 100000 : 25000)
                                  : profile_ == "m5" ? 0
                                                     : kBaseDelayUs;
    const int64_t delay_us = std::max<int64_t>(
        0, base_delay_us + jitter_us + (reordered ? kReorderDelayUs : 0));
    std::vector<uint8_t> payload(size);
    std::memcpy(payload.data(), bytes, size);
    queue_.push({.delivery_time =
                     next_serialization_ + std::chrono::microseconds(delay_us),
                 .bytes = std::move(payload),
                 .sequence = sequence});
    event_log_->Write(reordered ? "injected_reorder" : "relay_queued", name_,
                      sequence);
  }

  void Ingest(const std::array<uint8_t, kPacketSize>& packet, size_t size) {
    Ingest(packet.data(), size, PacketSequence(packet));
  }

  void DeliverDue(int socket_fd, const sockaddr_in& destination) {
    const auto now = SteadyClock::now();
    while (!queue_.empty() && queue_.top().delivery_time <= now) {
      const ScheduledPacket packet = queue_.top();
      queue_.pop();
      if (sendto(socket_fd, packet.bytes.data(), packet.bytes.size(), 0,
                 reinterpret_cast<const sockaddr*>(&destination),
                 sizeof(destination)) ==
          static_cast<ssize_t>(packet.bytes.size())) {
        ++result_->forwarded;
      }
      event_log_->Write("relay_egress", name_, packet.sequence);
    }
  }

  bool empty() const { return queue_.empty(); }

  void Clear() {
    queue_ = {};
    next_serialization_ = SteadyClock::now();
  }

 private:
  uint64_t BandwidthBps(SteadyClock::time_point now) const {
    if (profile_ == "m0" || profile_ == "m5")
      return kBitsPerSecond;
    const auto elapsed = now - profile_start_;
    if (profile_ == "m4") {
      if (elapsed < std::chrono::seconds(4))
        return 10'000'000;
      if (elapsed < std::chrono::seconds(12))
        return 512'000;
      return 8'000'000;
    }
    if (elapsed < std::chrono::seconds(3))
      return 10'000'000;
    if (elapsed < std::chrono::seconds(10))
      return 800'000;
    return 6'000'000;
  }

  bool IsM4Constrained(SteadyClock::time_point now) const {
    const auto elapsed = now - profile_start_;
    return profile_ == "m4" && elapsed >= std::chrono::seconds(4) &&
           elapsed < std::chrono::seconds(12);
  }

  const uint64_t seed_;
  const char* name_;
  EventLog* event_log_;
  UdpSelfTestResult* result_;
  const std::string profile_;
  const SteadyClock::time_point profile_start_;
  SteadyClock::time_point next_serialization_ = SteadyClock::now();
  std::priority_queue<ScheduledPacket,
                      std::vector<ScheduledPacket>,
                      EarliestPacketFirst>
      queue_;
};

bool ParseAddress(const std::string& value,
                  sockaddr_in* address,
                  std::string* error) {
  const size_t separator = value.rfind(':');
  if (separator == std::string::npos) {
    *error = "UDP address must be HOST:PORT";
    return false;
  }
  const std::string host = value.substr(0, separator);
  const std::string port_text = value.substr(separator + 1);
  char* end = nullptr;
  const unsigned long port = std::strtoul(port_text.c_str(), &end, 10);
  if (end == port_text.c_str() || *end != '\0' || port > 65535 || port == 0) {
    *error = "invalid UDP port";
    return false;
  }
  *address = {};
  address->sin_family = AF_INET;
  address->sin_port = htons(static_cast<uint16_t>(port));
  if (inet_pton(AF_INET, host.c_str(), &address->sin_addr) != 1) {
    *error = "invalid IPv4 address";
    return false;
  }
  return true;
}

Socket BoundSocketAt(const sockaddr_in& address, std::string* error) {
  Socket socket_fd(socket(AF_INET, SOCK_DGRAM, 0));
  if (socket_fd.value < 0 ||
      bind(socket_fd.value, reinterpret_cast<const sockaddr*>(&address),
           sizeof(address)) != 0 ||
      fcntl(socket_fd.value, F_SETFL, O_NONBLOCK) != 0) {
    *error = "failed to open relay UDP socket";
    return Socket();
  }
  return socket_fd;
}

bool SameAddress(const sockaddr_in& left, const sockaddr_in& right) {
  return left.sin_family == right.sin_family &&
         left.sin_port == right.sin_port &&
         left.sin_addr.s_addr == right.sin_addr.s_addr;
}

bool ContainsAddress(const std::vector<sockaddr_in>& addresses,
                     const sockaddr_in& candidate) {
  return std::any_of(addresses.begin(), addresses.end(), [&](const auto& item) {
    return SameAddress(item, candidate);
  });
}

void DrainIngress(int socket_fd, ImpairmentDirection* direction) {
  while (true) {
    std::array<uint8_t, kPacketSize> packet = {};
    size_t size = 0;
    if (!ReceivePacket(socket_fd, &packet, &size)) {
      return;
    }
    direction->Ingest(packet, size);
  }
}

}  // namespace

UdpSelfTestResult RunUdpEchoSelfTest(size_t packet_count) {
  UdpSelfTestResult result;
  sockaddr_in client_address = {};
  sockaddr_in server_address = {};
  Socket client = BoundSocket(&client_address, false, &result.error);
  Socket server = BoundSocket(&server_address, false, &result.error);
  if (client.value < 0 || server.value < 0) {
    return result;
  }

  for (uint32_t sequence = 0; sequence < packet_count; ++sequence) {
    const auto sent_packet = MakePacket(sequence);
    if (!SendPacket(client.value, server_address, sent_packet,
                    sent_packet.size())) {
      result.error = "failed to send UDP echo packet";
      return result;
    }
    ++result.sent;
    std::array<uint8_t, kPacketSize> received_packet = {};
    size_t received_size = 0;
    if (!ReceivePacket(server.value, &received_packet, &received_size) ||
        !SendPacket(server.value, client_address, received_packet,
                    received_size) ||
        !ReceivePacket(client.value, &received_packet, &received_size) ||
        PacketSequence(received_packet) != sequence) {
      result.error = "UDP echo packet mismatch";
      return result;
    }
    ++result.received;
  }
  return result;
}

UdpSelfTestResult RunUdpRelaySelfTest(size_t packet_count,
                                      const std::string& run_id,
                                      const std::string& event_log_path) {
  UdpSelfTestResult result;
  EventLog event_log(event_log_path, run_id);
  if (!event_log.ok()) {
    result.error = "failed to open relay event log";
    return result;
  }

  sockaddr_in client_address = {};
  sockaddr_in relay_forward_address = {};
  sockaddr_in relay_reverse_address = {};
  sockaddr_in server_address = {};
  Socket client = BoundSocket(&client_address, true, &result.error);
  Socket relay_forward =
      BoundSocket(&relay_forward_address, true, &result.error);
  Socket relay_reverse =
      BoundSocket(&relay_reverse_address, true, &result.error);
  Socket server = BoundSocket(&server_address, true, &result.error);
  if (client.value < 0 || relay_forward.value < 0 || relay_reverse.value < 0 ||
      server.value < 0) {
    return result;
  }

  ImpairmentDirection forward(1, "client_to_server", &event_log, &result);
  ImpairmentDirection reverse(1 ^ 0x9e3779b97f4a7c15ULL, "server_to_client",
                              &event_log, &result);
  std::vector<SteadyClock::time_point> sent_at(packet_count);
  for (uint32_t sequence = 0; sequence < packet_count; ++sequence) {
    const auto packet = MakePacket(sequence);
    sent_at[sequence] = SteadyClock::now();
    if (!SendPacket(client.value, relay_forward_address, packet,
                    packet.size())) {
      result.error = "failed to send UDP relay packet";
      return result;
    }
    ++result.sent;
    DrainIngress(relay_forward.value, &forward);
  }

  const auto deadline = SteadyClock::now() + std::chrono::seconds(5);
  std::vector<bool> received(packet_count, false);
  double total_rtt_ms = 0;
  result.min_rtt_ms = std::numeric_limits<double>::max();
  while (SteadyClock::now() < deadline) {
    DrainIngress(relay_forward.value, &forward);
    forward.DeliverDue(relay_forward.value, server_address);

    while (true) {
      std::array<uint8_t, kPacketSize> packet = {};
      size_t size = 0;
      if (!ReceivePacket(server.value, &packet, &size)) {
        break;
      }
      SendPacket(server.value, relay_reverse_address, packet, size);
    }

    DrainIngress(relay_reverse.value, &reverse);
    reverse.DeliverDue(relay_reverse.value, client_address);

    while (true) {
      std::array<uint8_t, kPacketSize> packet = {};
      size_t size = 0;
      if (!ReceivePacket(client.value, &packet, &size)) {
        break;
      }
      const uint32_t sequence = PacketSequence(packet);
      if (sequence < received.size() && !received[sequence]) {
        received[sequence] = true;
        ++result.received;
        const double rtt_ms = std::chrono::duration<double, std::milli>(
                                  SteadyClock::now() - sent_at[sequence])
                                  .count();
        total_rtt_ms += rtt_ms;
        result.min_rtt_ms = std::min(result.min_rtt_ms, rtt_ms);
        result.max_rtt_ms = std::max(result.max_rtt_ms, rtt_ms);
        event_log.Write("endpoint_receive", "server_to_client", sequence);
      }
    }

    if (forward.empty() && reverse.empty() &&
        result.received + result.injected_loss >= result.sent) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if (result.received == 0 || result.received >= result.sent ||
      result.injected_loss == 0 || result.injected_reorder == 0) {
    result.error = "M0 impairment profile did not exercise expected outcomes";
  } else {
    result.average_rtt_ms = total_rtt_ms / result.received;
  }
  return result;
}

UdpSelfTestResult RunUdpRelay(const std::string& listen_address,
                              const std::string& forward_address,
                              double duration_seconds,
                              const std::string& profile,
                              const std::string& run_id,
                              const std::string& event_log_path) {
  UdpSelfTestResult result;
  if (duration_seconds <= 0) {
    result.error = "relay duration must be positive";
    return result;
  }
  sockaddr_in listen = {};
  sockaddr_in forward = {};
  if (!ParseAddress(listen_address, &listen, &result.error) ||
      !ParseAddress(forward_address, &forward, &result.error)) {
    return result;
  }
  EventLog event_log(event_log_path, run_id);
  if (!event_log.ok()) {
    result.error = "failed to open relay event log";
    return result;
  }
  Socket client_side = BoundSocketAt(listen, &result.error);
  sockaddr_in ephemeral = {};
  Socket server_side = BoundSocket(&ephemeral, true, &result.error);
  if (client_side.value < 0 || server_side.value < 0) {
    return result;
  }
  std::cout << "{\"schema_version\":1,\"event\":\"ready\"}\n" << std::flush;

  ImpairmentDirection outbound(1, "client_to_server", &event_log, &result,
                               profile);
  ImpairmentDirection inbound(1 ^ 0x9e3779b97f4a7c15ULL, "server_to_client",
                              &event_log, &result, profile);
  sockaddr_in client = {};
  bool have_client = false;
  std::vector<sockaddr_in> retired_clients;
  uint32_t outbound_sequence = 0;
  uint32_t inbound_sequence = 0;
  const auto deadline = SteadyClock::now() +
                        std::chrono::microseconds(
                            static_cast<int64_t>(duration_seconds * 1000000.0));
  while (SteadyClock::now() < deadline || !outbound.empty() ||
         !inbound.empty()) {
    while (SteadyClock::now() < deadline) {
      std::array<uint8_t, kMaximumPacketSize> packet = {};
      sockaddr_in source = {};
      socklen_t client_length = sizeof(source);
      const ssize_t received =
          recvfrom(client_side.value, packet.data(), packet.size(), 0,
                   reinterpret_cast<sockaddr*>(&source), &client_length);
      if (received < 0) {
        break;
      }
      if (have_client && !SameAddress(client, source) &&
          ContainsAddress(retired_clients, source)) {
        continue;
      }
      if (have_client && !SameAddress(client, source)) {
        retired_clients.push_back(client);
        sockaddr_in replacement = {};
        server_side = BoundSocket(&replacement, true, &result.error);
        if (server_side.value < 0)
          return result;
        outbound.Clear();
        inbound.Clear();
      }
      client = source;
      have_client = true;
      ++result.sent;
      outbound.Ingest(packet.data(), static_cast<size_t>(received),
                      outbound_sequence++);
    }
    outbound.DeliverDue(server_side.value, forward);

    while (true) {
      std::array<uint8_t, kMaximumPacketSize> packet = {};
      const ssize_t received = recvfrom(server_side.value, packet.data(),
                                        packet.size(), 0, nullptr, nullptr);
      if (received < 0) {
        break;
      }
      ++result.received;
      inbound.Ingest(packet.data(), static_cast<size_t>(received),
                     inbound_sequence++);
    }
    if (have_client) {
      inbound.DeliverDue(client_side.value, client);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return result;
}

}  // namespace webrtc::roq
