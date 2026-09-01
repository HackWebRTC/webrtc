/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/roq/roq_peer.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"
#include "quiche.h"
#include "rtc_tools/roq/manifest.h"
#include "rtc_tools/roq/roq_wire.h"

namespace webrtc::roq {
namespace {

using SteadyClock = std::chrono::steady_clock;
constexpr std::array<uint8_t, 7> kAlpnWire = {6, 'r', 'o', 'q', '-', '1', '4'};
constexpr size_t kConnectionIdLength = 16;
constexpr size_t kPacketBufferSize = 65535;
constexpr size_t kMaximumUdpPayload = 1350;
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
  int value;
};

struct PendingPacket {
  std::vector<uint8_t> bytes;
  sockaddr_storage destination = {};
  socklen_t destination_length = 0;
  SteadyClock::time_point send_at;
};

struct FlowState {
  bool seen = false;
  uint16_t last_sequence = 0;
};

class EventLog {
 public:
  EventLog(const std::string& path, std::string run_id, PeerRole role)
      : output_(path),
        run_id_(std::move(run_id)),
        process_(role == PeerRole::kClient ? "client" : "server") {}

  bool ok() const { return output_.good(); }

  void Write(const char* event, const char* direction, uint64_t value = 0) {
    const auto monotonic_us =
        std::chrono::duration_cast<std::chrono::microseconds>(
            SteadyClock::now().time_since_epoch())
            .count();
    const auto utc_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    output_ << "{\"schema_version\":1,\"run_id\":\"" << run_id_
            << "\",\"process\":\"" << process_ << "\",\"event\":\"" << event
            << "\",\"direction\":\"" << direction
            << "\",\"sequence\":" << sequence_++ << ",\"value\":" << value
            << ",\"monotonic_us\":" << monotonic_us << ",\"utc_ms\":" << utc_ms
            << "}\n";
  }

 private:
  std::ofstream output_;
  std::string run_id_;
  std::string process_;
  uint64_t sequence_ = 0;
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
  errno = 0;
  const unsigned long port = std::strtoul(port_text.c_str(), &end, 10);
  if (errno != 0 || end == port_text.c_str() || *end != '\0' || port == 0 ||
      port > 65535) {
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

Socket OpenSocket(const sockaddr_in& bind_address,
                  sockaddr_in* actual_address,
                  std::string* error) {
  Socket socket_fd(socket(AF_INET, SOCK_DGRAM, 0));
  if (socket_fd.value < 0 ||
      bind(socket_fd.value, reinterpret_cast<const sockaddr*>(&bind_address),
           sizeof(bind_address)) != 0 ||
      fcntl(socket_fd.value, F_SETFL, O_NONBLOCK) != 0) {
    *error = "failed to open nonblocking UDP socket";
    return Socket();
  }
  socklen_t length = sizeof(*actual_address);
  if (getsockname(socket_fd.value, reinterpret_cast<sockaddr*>(actual_address),
                  &length) != 0) {
    *error = "failed to read local UDP address";
    return Socket();
  }
  return socket_fd;
}

ConfigPtr CreateConfig(const SyntheticRoqOptions& options, std::string* error) {
  ConfigPtr config(quiche_config_new(QUICHE_PROTOCOL_VERSION));
  if (!config ||
      quiche_config_set_application_protos(config.get(), kAlpnWire.data(),
                                           kAlpnWire.size()) != 0 ||
      quiche_config_set_cc_algorithm_name(config.get(), "bbr2_gcongestion") !=
          0) {
    *error = "failed to configure quiche";
    return nullptr;
  }
  quiche_config_set_max_idle_timeout(config.get(), 5000);
  quiche_config_set_max_recv_udp_payload_size(config.get(), kMaximumUdpPayload);
  quiche_config_set_max_send_udp_payload_size(config.get(), kMaximumUdpPayload);
  quiche_config_set_initial_max_data(config.get(), 1000000);
  quiche_config_set_initial_max_stream_data_bidi_local(config.get(), 10000);
  quiche_config_set_initial_max_stream_data_bidi_remote(config.get(), 10000);
  quiche_config_set_initial_max_streams_bidi(config.get(), 4);
  if (!options.disable_datagram) {
    quiche_config_enable_dgram(config.get(), true, 64, 8);
  }
  if (options.role == PeerRole::kClient) {
    quiche_config_verify_peer(config.get(), false);
  } else {
    const auto directory =
        std::filesystem::absolute(options.executable_path).parent_path();
    const std::string cert = (directory / "cert.crt").string();
    const std::string key = (directory / "cert.key").string();
    if (quiche_config_load_cert_chain_from_pem_file(config.get(),
                                                    cert.c_str()) != 0 ||
        quiche_config_load_priv_key_from_pem_file(config.get(), key.c_str()) !=
            0) {
      *error = "failed to load test certificate";
      return nullptr;
    }
  }
  return config;
}

uint32_t ReadUint32(std::span<const uint8_t> bytes, size_t offset) {
  return (static_cast<uint32_t>(bytes[offset]) << 24) |
         (static_cast<uint32_t>(bytes[offset + 1]) << 16) |
         (static_cast<uint32_t>(bytes[offset + 2]) << 8) | bytes[offset + 3];
}

void AppendUint16(uint16_t value, std::vector<uint8_t>* output) {
  output->push_back(static_cast<uint8_t>(value >> 8));
  output->push_back(static_cast<uint8_t>(value));
}

void AppendUint32(uint32_t value, std::vector<uint8_t>* output) {
  output->push_back(static_cast<uint8_t>(value >> 24));
  output->push_back(static_cast<uint8_t>(value >> 16));
  output->push_back(static_cast<uint8_t>(value >> 8));
  output->push_back(static_cast<uint8_t>(value));
}

std::vector<uint8_t> MakeRtp(uint8_t payload_type,
                             uint32_t ssrc,
                             uint16_t sequence,
                             uint32_t timestamp,
                             uint8_t media_tag) {
  std::vector<uint8_t> packet;
  packet.reserve(32);
  packet.push_back(0x80);
  packet.push_back(payload_type);
  AppendUint16(sequence, &packet);
  AppendUint32(timestamp, &packet);
  AppendUint32(ssrc, &packet);
  packet.push_back('R');
  packet.push_back('O');
  packet.push_back('Q');
  packet.push_back(media_tag);
  AppendUint16(sequence, &packet);
  return packet;
}

std::vector<uint8_t> MakeRtcp(uint32_t sender_ssrc,
                              uint32_t peer_audio_ssrc,
                              uint32_t peer_video_ssrc) {
  std::vector<uint8_t> packet;
  packet.reserve(76);
  packet.push_back(0x82);
  packet.push_back(201);
  AppendUint16(13, &packet);
  AppendUint32(sender_ssrc, &packet);
  for (uint32_t source : {peer_audio_ssrc, peer_video_ssrc}) {
    AppendUint32(source, &packet);
    for (int i = 0; i < 20; ++i) {
      packet.push_back(0);
    }
  }
  constexpr std::array<uint8_t, 8> kCname = {'r', 'o', 'q', '-',
                                             'p', 'e', 'e', 'r'};
  packet.push_back(0x81);
  packet.push_back(202);
  AppendUint16(4, &packet);
  AppendUint32(sender_ssrc, &packet);
  packet.push_back(1);
  packet.push_back(kCname.size());
  packet.insert(packet.end(), kCname.begin(), kCname.end());
  packet.push_back(0);
  while (packet.size() % 4 != 0) {
    packet.push_back(0);
  }
  return packet;
}

bool UpdateSequence(uint16_t sequence,
                    FlowState* state,
                    SyntheticRoqResult* result) {
  if (state->seen) {
    if (sequence == state->last_sequence) {
      ++result->duplicate;
      return false;
    }
    if (static_cast<int16_t>(sequence - state->last_sequence) < 0) {
      ++result->reordered;
    }
  }
  state->seen = true;
  state->last_sequence = sequence;
  return true;
}

bool ValidateRtp(const RoqDatagram& datagram,
                 const ManifestMedia& media,
                 uint32_t remote_ssrc,
                 FlowState* state,
                 SyntheticRoqResult* result) {
  RtpPacket parsed;
  if (!parsed.Parse(datagram.packet) ||
      parsed.PayloadType() != media.payload_type ||
      parsed.Ssrc() != remote_ssrc) {
    return false;
  }
  UpdateSequence(parsed.SequenceNumber(), state, result);
  return true;
}

bool ValidateRtcp(const RoqDatagram& datagram,
                  uint32_t remote_sender_ssrc,
                  uint32_t local_audio_ssrc,
                  uint32_t local_video_ssrc) {
  if (datagram.packet.size() != 76) {
    return false;
  }
  rtcp::CommonHeader receiver_report;
  if (!receiver_report.Parse(datagram.packet.data(), datagram.packet.size()) ||
      receiver_report.type() != 201 || receiver_report.count() != 2 ||
      receiver_report.packet_size() != 56 ||
      ReadUint32(datagram.packet, 4) != remote_sender_ssrc ||
      ReadUint32(datagram.packet, 8) != local_audio_ssrc ||
      ReadUint32(datagram.packet, 32) != local_video_ssrc) {
    return false;
  }
  const std::span<const uint8_t> sdes_packet = datagram.packet.subspan(56);
  rtcp::CommonHeader sdes;
  return sdes.Parse(sdes_packet.data(), sdes_packet.size()) &&
         sdes.type() == 202 && sdes.count() == 1 &&
         sdes.packet_size() == sdes_packet.size() &&
         ReadUint32(sdes_packet, 4) == remote_sender_ssrc;
}

uint32_t SsrcForRole(const ManifestMedia& media, PeerRole role) {
  return role == PeerRole::kClient ? media.client_ssrc : media.server_ssrc;
}

bool CloseConnection(quiche_conn* connection,
                     RoqError error,
                     std::string_view reason) {
  return quiche_conn_close(connection, true, static_cast<uint64_t>(error),
                           reinterpret_cast<const uint8_t*>(reason.data()),
                           reason.size()) == 0;
}

bool HandleDatagrams(quiche_conn* connection,
                     const SessionManifest& manifest,
                     PeerRole role,
                     SyntheticRoqResult* result,
                     EventLog* event_log,
                     FlowState* audio_state,
                     FlowState* video_state,
                     bool in_final_second) {
  std::vector<uint8_t> buffer;
  const PeerRole remote_role =
      role == PeerRole::kClient ? PeerRole::kServer : PeerRole::kClient;
  while (true) {
    const ssize_t next_length = quiche_conn_dgram_recv_front_len(connection);
    if (next_length == QUICHE_ERR_DONE) {
      return true;
    }
    if (next_length < 0) {
      result->error = "failed to inspect received DATAGRAM";
      return false;
    }
    buffer.resize(static_cast<size_t>(next_length));
    const ssize_t read =
        quiche_conn_dgram_recv(connection, buffer.data(), buffer.size());
    if (read < 0) {
      result->error = "failed to receive DATAGRAM";
      return false;
    }
    RoqDatagram datagram;
    std::string parse_error;
    if (!DecodeRoqDatagram(
            std::span<const uint8_t>(buffer.data(), static_cast<size_t>(read)),
            &datagram, &parse_error)) {
      ++result->malformed;
      CloseConnection(connection, RoqError::kPacketError, parse_error);
      return true;
    }
    if (datagram.flow_id == manifest.audio.flow_id) {
      if (!ValidateRtp(datagram, manifest.audio,
                       SsrcForRole(manifest.audio, remote_role), audio_state,
                       result)) {
        ++result->malformed;
        CloseConnection(connection, RoqError::kPacketError,
                        "invalid audio RTP");
        return true;
      }
      ++result->received_audio_rtp;
      result->audio_received_in_final_second |= in_final_second;
      event_log->Write("datagram_received", "audio_rtp",
                       result->received_audio_rtp);
    } else if (datagram.flow_id == manifest.video.flow_id) {
      if (!ValidateRtp(datagram, manifest.video,
                       SsrcForRole(manifest.video, remote_role), video_state,
                       result)) {
        ++result->malformed;
        CloseConnection(connection, RoqError::kPacketError,
                        "invalid video RTP");
        return true;
      }
      ++result->received_video_rtp;
      result->video_received_in_final_second |= in_final_second;
      event_log->Write("datagram_received", "video_rtp",
                       result->received_video_rtp);
    } else if (datagram.flow_id == manifest.rtcp_flow_id) {
      if (!ValidateRtcp(datagram, SsrcForRole(manifest.audio, remote_role),
                        SsrcForRole(manifest.audio, role),
                        SsrcForRole(manifest.video, role))) {
        ++result->malformed;
        CloseConnection(connection, RoqError::kPacketError, "invalid RTCP");
        return true;
      }
      ++result->received_rtcp;
      event_log->Write("datagram_received", "rtcp", result->received_rtcp);
    } else {
      ++result->unknown_flow;
      CloseConnection(connection, RoqError::kUnknownFlowId,
                      "unknown RoQ flow ID");
      return true;
    }
  }
}

bool QueueRoq(quiche_conn* connection,
              std::vector<uint8_t> payload,
              size_t* generated,
              SyntheticRoqResult* result,
              EventLog* event_log,
              const char* kind) {
  ++*generated;
  const ssize_t queued =
      quiche_conn_dgram_send(connection, payload.data(), payload.size());
  if (queued == QUICHE_ERR_DONE) {
    ++result->queue_full_drop;
    event_log->Write("queue_full_drop", kind, *generated);
    return true;
  }
  if (queued != static_cast<ssize_t>(payload.size())) {
    result->error = "failed to queue " + std::string(kind) +
                    " DATAGRAM: " + std::to_string(queued);
    return false;
  }
  event_log->Write("datagram_queued", kind, *generated);
  return true;
}

bool QueueScheduledMedia(quiche_conn* connection,
                         const SessionManifest& manifest,
                         PeerRole role,
                         SteadyClock::time_point media_start,
                         SteadyClock::time_point now,
                         SyntheticRoqResult* result,
                         EventLog* event_log) {
  const auto elapsed_us =
      std::chrono::duration_cast<std::chrono::microseconds>(now - media_start)
          .count();
  const size_t target_audio = static_cast<size_t>(elapsed_us / 10000) + 1;
  const size_t target_video =
      static_cast<size_t>(elapsed_us * 60 / 1000000) + 1;
  const size_t target_rtcp = static_cast<size_t>(elapsed_us * 5 / 1000000) + 1;
  const uint32_t audio_ssrc = SsrcForRole(manifest.audio, role);
  const uint32_t video_ssrc = SsrcForRole(manifest.video, role);
  const PeerRole remote =
      role == PeerRole::kClient ? PeerRole::kServer : PeerRole::kClient;
  while (result->generated_audio_rtp < target_audio) {
    const uint16_t sequence =
        static_cast<uint16_t>(result->generated_audio_rtp);
    if (!QueueRoq(
            connection,
            EncodeRoqDatagram(manifest.audio.flow_id,
                              MakeRtp(manifest.audio.payload_type, audio_ssrc,
                                      sequence, sequence * 480, 'A')),
            &result->generated_audio_rtp, result, event_log, "audio_rtp")) {
      return false;
    }
  }
  while (result->generated_video_rtp < target_video) {
    const uint16_t sequence =
        static_cast<uint16_t>(result->generated_video_rtp);
    if (!QueueRoq(
            connection,
            EncodeRoqDatagram(manifest.video.flow_id,
                              MakeRtp(manifest.video.payload_type, video_ssrc,
                                      sequence, sequence * 1500, 'V')),
            &result->generated_video_rtp, result, event_log, "video_rtp")) {
      return false;
    }
  }
  while (result->generated_rtcp < target_rtcp) {
    if (!QueueRoq(connection,
                  EncodeRoqDatagram(
                      manifest.rtcp_flow_id,
                      MakeRtcp(audio_ssrc, SsrcForRole(manifest.audio, remote),
                               SsrcForRole(manifest.video, remote))),
                  &result->generated_rtcp, result, event_log, "rtcp")) {
      return false;
    }
  }
  return true;
}

bool InjectError(quiche_conn* connection,
                 const SyntheticRoqOptions& options,
                 const SessionManifest& manifest,
                 SyntheticRoqResult* result,
                 EventLog* event_log) {
  std::vector<uint8_t> payload;
  if (options.inject_error == "unknown-flow") {
    payload = EncodeRoqDatagram(63, MakeRtp(111, 1, 1, 480, 'A'));
  } else if (options.inject_error == "malformed-varint") {
    payload = {0x40};
  } else if (options.inject_error == "invalid-packet") {
    constexpr std::array<uint8_t, 2> kInvalidRtp = {0x80, 111};
    payload = EncodeRoqDatagram(manifest.audio.flow_id, kInvalidRtp);
  } else if (options.inject_error == "flow-mismatch") {
    payload = EncodeRoqDatagram(
        manifest.audio.flow_id,
        MakeRtp(manifest.video.payload_type,
                SsrcForRole(manifest.video, options.role), 1, 1500, 'V'));
  } else if (options.inject_error == "invalid-rtcp") {
    const PeerRole remote = options.role == PeerRole::kClient
                                ? PeerRole::kServer
                                : PeerRole::kClient;
    std::vector<uint8_t> packet =
        MakeRtcp(SsrcForRole(manifest.audio, options.role),
                 SsrcForRole(manifest.audio, remote),
                 SsrcForRole(manifest.video, remote));
    packet[58] = 0;
    packet[59] = 20;
    payload = EncodeRoqDatagram(manifest.rtcp_flow_id, packet);
  } else {
    return true;
  }
  size_t generated = 0;
  return QueueRoq(connection, std::move(payload), &generated, result, event_log,
                  "injected_error");
}

void CaptureClose(quiche_conn* connection, SyntheticRoqResult* result) {
  bool is_app = false;
  uint64_t code = 0;
  const uint8_t* reason = nullptr;
  size_t reason_length = 0;
  if (quiche_conn_peer_error(connection, &is_app, &code, &reason,
                             &reason_length) ||
      quiche_conn_local_error(connection, &is_app, &code, &reason,
                              &reason_length)) {
    result->close_is_application = is_app;
    result->close_code = code;
    result->close_observed = true;
  }
}

}  // namespace

SyntheticRoqResult RunSyntheticRoq(const SyntheticRoqOptions& options) {
  SyntheticRoqResult result;
  SessionManifest manifest;
  if (!LoadSessionManifest(options.manifest_path, &manifest, &result.error)) {
    return result;
  }
  EventLog event_log(options.event_log_path, options.run_id, options.role);
  if (!event_log.ok()) {
    result.error = "failed to open peer event log";
    return result;
  }
  sockaddr_in configured_address = {};
  if (!ParseAddress(options.address, &configured_address, &result.error)) {
    return result;
  }
  sockaddr_in bind_address = configured_address;
  if (options.role == PeerRole::kClient) {
    bind_address.sin_port = 0;
  }
  sockaddr_in local_address = {};
  Socket socket_fd = OpenSocket(bind_address, &local_address, &result.error);
  if (socket_fd.value < 0) {
    return result;
  }
  ConfigPtr config = CreateConfig(options, &result.error);
  if (!config) {
    return result;
  }

  ConnectionPtr connection;
  if (options.role == PeerRole::kClient) {
    std::array<uint8_t, kConnectionIdLength> scid = {};
    for (size_t i = 0; i < scid.size(); ++i) {
      scid[i] = static_cast<uint8_t>(0x90 + i);
    }
    connection.reset(
        quiche_connect("quic.tech", scid.data(), scid.size(),
                       reinterpret_cast<const sockaddr*>(&local_address),
                       sizeof(local_address),
                       reinterpret_cast<const sockaddr*>(&configured_address),
                       sizeof(configured_address), config.get()));
    if (!connection) {
      result.error = "failed to create client connection";
      return result;
    }
  } else {
    std::cout << "{\"schema_version\":1,\"event\":\"ready\"}\n" << std::flush;
  }

  std::optional<PendingPacket> pending_packet;
  std::optional<SteadyClock::time_point> media_start;
  bool injected = false;
  bool close_started = false;
  bool close_egress_drained = false;
  bool negotiation_checked = false;
  FlowState audio_state;
  FlowState video_state;
  const auto hard_deadline =
      SteadyClock::now() + std::chrono::milliseconds(static_cast<int64_t>(
                               options.duration_seconds * 1000 + 10000));
  std::array<uint8_t, kPacketBufferSize> packet = {};

  while (SteadyClock::now() < hard_deadline) {
    while (true) {
      sockaddr_storage peer_address = {};
      socklen_t peer_address_length = sizeof(peer_address);
      const ssize_t received = recvfrom(
          socket_fd.value, packet.data(), packet.size(), 0,
          reinterpret_cast<sockaddr*>(&peer_address), &peer_address_length);
      if (received < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
          result.error = "UDP receive failed";
          return result;
        }
        break;
      }
      event_log.Write("quic_packet_received", "ingress",
                      static_cast<uint64_t>(received));
      if (!connection) {
        uint32_t version = 0;
        uint8_t type = 0;
        std::array<uint8_t, QUICHE_MAX_CONN_ID_LEN> scid = {};
        size_t scid_length = scid.size();
        std::array<uint8_t, QUICHE_MAX_CONN_ID_LEN> dcid = {};
        size_t dcid_length = dcid.size();
        std::array<uint8_t, 256> token = {};
        size_t token_length = token.size();
        if (quiche_header_info(
                packet.data(), static_cast<size_t>(received),
                kConnectionIdLength, &version, &type, scid.data(), &scid_length,
                dcid.data(), &dcid_length, token.data(), &token_length) != 0 ||
            !quiche_version_is_supported(version)) {
          continue;
        }
        connection.reset(quiche_accept(
            dcid.data(), dcid_length, nullptr, 0,
            reinterpret_cast<const sockaddr*>(&local_address),
            sizeof(local_address), reinterpret_cast<sockaddr*>(&peer_address),
            peer_address_length, config.get()));
        if (!connection) {
          result.error = "failed to accept server connection";
          return result;
        }
      }
      quiche_recv_info recv_info = {
          .from = reinterpret_cast<sockaddr*>(&peer_address),
          .from_len = peer_address_length,
          .to = reinterpret_cast<sockaddr*>(&local_address),
          .to_len = sizeof(local_address),
      };
      const ssize_t processed =
          quiche_conn_recv(connection.get(), packet.data(),
                           static_cast<size_t>(received), &recv_info);
      event_log.Write("quiche_recv_result", "ingress",
                      processed < 0 ? static_cast<uint64_t>(-processed)
                                    : static_cast<uint64_t>(processed));
      if (processed < 0 && processed != QUICHE_ERR_DONE) {
        result.error = "quiche receive failed: " + std::to_string(processed);
        return result;
      }
      const bool in_final_second =
          media_start &&
          SteadyClock::now() - *media_start >=
              std::chrono::microseconds(static_cast<int64_t>(
                  std::max(0.0, options.duration_seconds - 1.0) * 1000000));
      if (!HandleDatagrams(connection.get(), manifest, options.role, &result,
                           &event_log, &audio_state, &video_state,
                           in_final_second)) {
        return result;
      }
      bool peer_is_app = false;
      uint64_t peer_code = 0;
      const uint8_t* peer_reason = nullptr;
      size_t peer_reason_length = 0;
      if (quiche_conn_peer_error(connection.get(), &peer_is_app, &peer_code,
                                 &peer_reason, &peer_reason_length)) {
        result.close_is_application = peer_is_app;
        result.close_code = peer_code;
        result.close_observed = true;
        event_log.Write("peer_close_observed", "ingress", peer_code);
        close_started = true;
        close_egress_drained = false;
      }
    }

    const auto now = SteadyClock::now();
    if (connection && quiche_conn_is_established(connection.get()) &&
        !negotiation_checked) {
      negotiation_checked = true;
      const uint8_t* alpn = nullptr;
      size_t alpn_length = 0;
      quiche_conn_application_proto(connection.get(), &alpn, &alpn_length);
      result.alpn.assign(reinterpret_cast<const char*>(alpn), alpn_length);
      quiche_transport_params peer_params = {};
      result.datagram_negotiated =
          !options.disable_datagram &&
          quiche_conn_peer_transport_params(connection.get(), &peer_params) &&
          peer_params.peer_max_datagram_frame_size > 0;
      event_log.Write("handshake_established", "local");
      if (!result.datagram_negotiated && options.disable_datagram) {
        CloseConnection(connection.get(), RoqError::kExpectationUnmet,
                        "DATAGRAM was not negotiated");
        close_started = true;
        close_egress_drained = false;
      } else if (result.datagram_negotiated) {
        media_start = now;
      }
    }

    if (connection && media_start && !close_started) {
      const auto media_end =
          *media_start + std::chrono::microseconds(static_cast<int64_t>(
                             options.duration_seconds * 1000000));
      if (now < media_end) {
        if (!QueueScheduledMedia(connection.get(), manifest, options.role,
                                 *media_start, now, &result, &event_log)) {
          return result;
        }
        if (!injected && !options.inject_error.empty() &&
            now - *media_start > std::chrono::milliseconds(100)) {
          if (!InjectError(connection.get(), options, manifest, &result,
                           &event_log)) {
            return result;
          }
          injected = true;
        }
      } else if (options.role == PeerRole::kClient &&
                 now - media_end > std::chrono::seconds(1)) {
        CloseConnection(connection.get(), RoqError::kNoError, "complete");
        close_started = true;
        close_egress_drained = false;
      }
    }

    if (connection) {
      const uint64_t timeout_ns =
          quiche_conn_timeout_as_nanos(connection.get());
      if (timeout_ns != kNoTimeout && timeout_ns == 0) {
        quiche_conn_on_timeout(connection.get());
      }
    }

    if (pending_packet && now >= pending_packet->send_at) {
      const ssize_t sent = sendto(
          socket_fd.value, pending_packet->bytes.data(),
          pending_packet->bytes.size(), 0,
          reinterpret_cast<const sockaddr*>(&pending_packet->destination),
          pending_packet->destination_length);
      if (sent != static_cast<ssize_t>(pending_packet->bytes.size())) {
        result.error = "UDP send failed";
        return result;
      }
      event_log.Write("quic_packet_sent", "egress",
                      pending_packet->bytes.size());
      pending_packet.reset();
    }
    if (connection && !pending_packet) {
      quiche_send_info send_info = {};
      const ssize_t written = quiche_conn_send(connection.get(), packet.data(),
                                               kMaximumUdpPayload, &send_info);
      if (written >= 0) {
        PendingPacket pending;
        pending.bytes.assign(packet.begin(),
                             packet.begin() + static_cast<size_t>(written));
        pending.destination = send_info.to;
        pending.destination_length = send_info.to_len;
        pending.send_at =
            now + std::chrono::nanoseconds(send_info.pacing_delay_nanos);
        result.max_pacing_delay_ns =
            std::max(result.max_pacing_delay_ns, send_info.pacing_delay_nanos);
        pending_packet = std::move(pending);
      } else if (written != QUICHE_ERR_DONE) {
        result.error = "quiche send failed: " + std::to_string(written);
        return result;
      } else if (close_started) {
        close_egress_drained = true;
      }
    }

    if (connection && quiche_conn_is_closed(connection.get()) &&
        !pending_packet && (!close_started || close_egress_drained)) {
      CaptureClose(connection.get(), &result);
      break;
    }
    if (close_started && close_egress_drained && !pending_packet) {
      CaptureClose(connection.get(), &result);
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  if (!connection) {
    result.error = "no QUIC connection was created";
    return result;
  }
  if (!media_start && result.close_code == 0) {
    result.error = "QUIC handshake did not establish media session";
    return result;
  }
  quiche_path_stats path = {};
  if (quiche_conn_path_stats(connection.get(), 0, &path) == 0) {
    result.rtt_ns = path.rtt;
    result.max_rtt_ns = path.max_rtt;
    result.delivery_rate_bytes_per_second = path.delivery_rate;
    result.max_bandwidth_bytes_per_second = path.max_bandwidth;
    result.pmtu_bytes = path.pmtu;
    result.dgram_lost = path.dgram_lost;
  }
  CaptureClose(connection.get(), &result);
  return result;
}

}  // namespace webrtc::roq
