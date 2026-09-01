/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/roq/quiche_abi.h"

#include <arpa/inet.h>
#include <sys/socket.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "quiche.h"

namespace webrtc::roq {
namespace {

constexpr std::array<uint8_t, 7> kAlpnWire = {6, 'r', 'o', 'q', '-', '1', '4'};
constexpr std::array<uint8_t, 48> kTicketKey = {
    0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
    0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
    0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
    0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42};
constexpr size_t kPacketBufferSize = 65535;

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

struct ConnectionPair {
  ConnectionPtr client;
  ConnectionPtr server;
  sockaddr_in client_address = {};
  sockaddr_in server_address = {};
};

sockaddr_in LoopbackAddress(uint16_t port) {
  sockaddr_in address = {};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  return address;
}

ConfigPtr CreateConfig(const std::filesystem::path& executable_dir,
                       bool server,
                       uint64_t initial_max_data,
                       std::string* error) {
  ConfigPtr config(quiche_config_new(QUICHE_PROTOCOL_VERSION));
  if (!config ||
      quiche_config_set_application_protos(config.get(), kAlpnWire.data(),
                                           kAlpnWire.size()) != 0 ||
      quiche_config_set_cc_algorithm_name(config.get(), "bbr2_gcongestion") !=
          0) {
    *error = "failed to create BBR2 quiche config";
    return nullptr;
  }
  quiche_config_set_max_idle_timeout(config.get(), 5000);
  quiche_config_set_max_recv_udp_payload_size(config.get(), 1350);
  quiche_config_set_max_send_udp_payload_size(config.get(), 1350);
  quiche_config_set_initial_max_data(config.get(), initial_max_data);
  quiche_config_set_initial_max_stream_data_bidi_local(config.get(), 10000);
  quiche_config_set_initial_max_stream_data_bidi_remote(config.get(), 10000);
  quiche_config_set_initial_max_streams_bidi(config.get(), 4);
  quiche_config_enable_dgram(config.get(), true, 256, 256);
  quiche_config_enable_early_data(config.get());
  if (quiche_config_set_ticket_key(config.get(), kTicketKey.data(),
                                   kTicketKey.size()) != 0) {
    *error = "failed to configure session ticket key";
    return nullptr;
  }
  if (server) {
    const std::string cert_path = (executable_dir / "cert.crt").string();
    const std::string key_path = (executable_dir / "cert.key").string();
    if (quiche_config_load_cert_chain_from_pem_file(config.get(),
                                                    cert_path.c_str()) != 0 ||
        quiche_config_load_priv_key_from_pem_file(config.get(),
                                                  key_path.c_str()) != 0) {
      *error = "failed to load test certificate";
      return nullptr;
    }
  } else {
    quiche_config_verify_peer(config.get(), false);
  }
  return config;
}

ConnectionPair CreatePair(quiche_config* client_config,
                          quiche_config* server_config,
                          uint8_t connection_index,
                          std::string* error) {
  ConnectionPair pair;
  pair.client_address = LoopbackAddress(12000 + connection_index * 2);
  pair.server_address = LoopbackAddress(12001 + connection_index * 2);
  std::array<uint8_t, 16> client_scid = {};
  for (size_t i = 0; i < client_scid.size(); ++i) {
    client_scid[i] = static_cast<uint8_t>(connection_index * 17 + i);
  }
  pair.client.reset(
      quiche_connect("quic.tech", client_scid.data(), client_scid.size(),
                     reinterpret_cast<const sockaddr*>(&pair.client_address),
                     sizeof(pair.client_address),
                     reinterpret_cast<const sockaddr*>(&pair.server_address),
                     sizeof(pair.server_address), client_config));
  if (!pair.client) {
    *error = "failed to create quiche client";
    return pair;
  }

  const uint8_t* client_dcid = nullptr;
  size_t client_dcid_length = 0;
  quiche_conn_destination_id(pair.client.get(), &client_dcid,
                             &client_dcid_length);
  pair.server.reset(
      quiche_accept(client_dcid, client_dcid_length, nullptr, 0,
                    reinterpret_cast<const sockaddr*>(&pair.server_address),
                    sizeof(pair.server_address),
                    reinterpret_cast<const sockaddr*>(&pair.client_address),
                    sizeof(pair.client_address), server_config));
  if (!pair.server) {
    *error = "failed to create quiche server";
  }
  return pair;
}

bool TransferPackets(quiche_conn* sender,
                     quiche_conn* receiver,
                     bool drop_first,
                     uint64_t* max_pacing_delay_ns,
                     std::string* error) {
  std::array<uint8_t, kPacketBufferSize> packet = {};
  bool dropped = false;
  while (true) {
    quiche_send_info send_info = {};
    const ssize_t written =
        quiche_conn_send(sender, packet.data(), packet.size(), &send_info);
    if (written == QUICHE_ERR_DONE) {
      return true;
    }
    if (written < 0) {
      *error = "quiche_conn_send failed: " + std::to_string(written);
      return false;
    }
    *max_pacing_delay_ns =
        std::max(*max_pacing_delay_ns, send_info.pacing_delay_nanos);
    if (drop_first && !dropped) {
      dropped = true;
      continue;
    }
    quiche_recv_info recv_info = {
        .from = reinterpret_cast<sockaddr*>(&send_info.from),
        .from_len = send_info.from_len,
        .to = reinterpret_cast<sockaddr*>(&send_info.to),
        .to_len = send_info.to_len,
    };
    const ssize_t received = quiche_conn_recv(
        receiver, packet.data(), static_cast<size_t>(written), &recv_info);
    if (received < 0) {
      *error = "quiche_conn_recv failed: " + std::to_string(received);
      return false;
    }
  }
}

bool ExchangeUntilEstablished(ConnectionPair* pair,
                              uint64_t* max_pacing_delay_ns,
                              std::string* error) {
  for (int round = 0; round < 1000; ++round) {
    if (!TransferPackets(pair->client.get(), pair->server.get(), false,
                         max_pacing_delay_ns, error) ||
        !TransferPackets(pair->server.get(), pair->client.get(), false,
                         max_pacing_delay_ns, error)) {
      return false;
    }
    if (quiche_conn_is_established(pair->client.get()) &&
        quiche_conn_is_established(pair->server.get())) {
      // Drain post-handshake tickets and acknowledgements as well.
      for (int drain = 0; drain < 4; ++drain) {
        if (!TransferPackets(pair->client.get(), pair->server.get(), false,
                             max_pacing_delay_ns, error) ||
            !TransferPackets(pair->server.get(), pair->client.get(), false,
                             max_pacing_delay_ns, error)) {
          return false;
        }
      }
      return true;
    }
  }
  *error = "QUIC handshake did not complete";
  return false;
}

std::vector<uint8_t> CopySession(quiche_conn* client) {
  const uint8_t* data = nullptr;
  size_t length = 0;
  quiche_conn_session(client, &data, &length);
  if (data == nullptr) {
    return {};
  }
  const std::span<const uint8_t> session(data, length);
  return std::vector<uint8_t>(session.begin(), session.end());
}

bool QueueDatagrams(quiche_conn* connection, size_t count, std::string* error) {
  std::array<uint8_t, 1000> payload = {};
  for (size_t i = 0; i < count; ++i) {
    payload[0] = static_cast<uint8_t>(i);
    const ssize_t result =
        quiche_conn_dgram_send(connection, payload.data(), payload.size());
    if (result != static_cast<ssize_t>(payload.size())) {
      *error = "failed to queue DATAGRAM: " + std::to_string(result);
      return false;
    }
  }
  return true;
}

bool ExercisePathStats(ConnectionPair* pair,
                       QuicheAbiResult* result,
                       std::string* error) {
  if (!QueueDatagrams(pair->client.get(), 64, error) ||
      !TransferPackets(pair->client.get(), pair->server.get(), true,
                       &result->max_pacing_delay_ns, error)) {
    return false;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  if (!TransferPackets(pair->server.get(), pair->client.get(), false,
                       &result->max_pacing_delay_ns, error) ||
      !QueueDatagrams(pair->client.get(), 16, error) ||
      !TransferPackets(pair->client.get(), pair->server.get(), false,
                       &result->max_pacing_delay_ns, error) ||
      !TransferPackets(pair->server.get(), pair->client.get(), false,
                       &result->max_pacing_delay_ns, error)) {
    return false;
  }

  quiche_path_stats stats = {};
  if (quiche_conn_path_stats(pair->client.get(), 0, &stats) != 0) {
    *error = "failed to read quiche path stats";
    return false;
  }
  result->rtt_ns = stats.rtt;
  result->min_rtt_ns = stats.min_rtt;
  result->max_rtt_ns = stats.max_rtt;
  result->delivery_rate_bytes_per_second = stats.delivery_rate;
  result->max_bandwidth_bytes_per_second = stats.max_bandwidth;
  result->pmtu_bytes = stats.pmtu;
  result->dgram_lost = stats.dgram_lost;
  return true;
}

bool ExerciseEarlyData(const std::filesystem::path& executable_dir,
                       bool mismatch_server_transport_parameters,
                       uint8_t connection_index,
                       uint32_t* reason,
                       bool* established,
                       std::string* error) {
  ConfigPtr first_client = CreateConfig(executable_dir, false, 100000, error);
  ConfigPtr first_server = CreateConfig(executable_dir, true, 100000, error);
  if (!first_client || !first_server) {
    return false;
  }
  ConnectionPair first = CreatePair(first_client.get(), first_server.get(),
                                    connection_index, error);
  uint64_t ignored_pacing_delay = 0;
  if (!first.client || !first.server ||
      !ExchangeUntilEstablished(&first, &ignored_pacing_delay, error)) {
    return false;
  }
  const std::vector<uint8_t> session = CopySession(first.client.get());
  if (session.empty()) {
    *error = "initial handshake produced no resumable session";
    return false;
  }

  ConfigPtr resumed_client = CreateConfig(executable_dir, false, 100000, error);
  ConfigPtr resumed_server = CreateConfig(
      executable_dir, true,
      mismatch_server_transport_parameters ? 200000 : 100000, error);
  if (!resumed_client || !resumed_server) {
    return false;
  }
  ConnectionPair resumed = CreatePair(
      resumed_client.get(), resumed_server.get(), connection_index + 1, error);
  if (!resumed.client || !resumed.server ||
      quiche_conn_set_session(resumed.client.get(), session.data(),
                              session.size()) != 0) {
    *error = "failed to configure resumed session";
    return false;
  }
  const std::array<uint8_t, 32> early_payload = {0x01};
  if (!TransferPackets(resumed.client.get(), resumed.server.get(), false,
                       &ignored_pacing_delay, error)) {
    return false;
  }
  const ssize_t early_result = quiche_conn_dgram_send(
      resumed.client.get(), early_payload.data(), early_payload.size());
  if (early_result != static_cast<ssize_t>(early_payload.size())) {
    *error = "failed to queue early DATAGRAM: " + std::to_string(early_result);
    return false;
  }
  if (!ExchangeUntilEstablished(&resumed, &ignored_pacing_delay, error)) {
    return false;
  }
  *reason = quiche_conn_early_data_reason(resumed.server.get());
  *established = quiche_conn_is_established(resumed.client.get()) &&
                 quiche_conn_is_established(resumed.server.get());
  if (!*established) {
    bool is_app = false;
    uint64_t code = 0;
    const uint8_t* reason_bytes = nullptr;
    size_t reason_length = 0;
    const bool client_local_error = quiche_conn_local_error(
        resumed.client.get(), &is_app, &code, &reason_bytes, &reason_length);
    *error = "resumed connection lost established state: client=" +
             std::to_string(quiche_conn_is_established(resumed.client.get())) +
             " server=" +
             std::to_string(quiche_conn_is_established(resumed.server.get())) +
             " client_resumed=" +
             std::to_string(quiche_conn_is_resumed(resumed.client.get())) +
             " server_resumed=" +
             std::to_string(quiche_conn_is_resumed(resumed.server.get())) +
             " server_early_reason=" + std::to_string(*reason) +
             " client_local_error=" + std::to_string(client_local_error) +
             " code=" + std::to_string(code);
  }
  return true;
}

}  // namespace

QuicheAbiResult RunQuicheAbiChecks(const std::string& executable_path) {
  QuicheAbiResult result;
  const std::filesystem::path executable_dir =
      std::filesystem::absolute(executable_path).parent_path();

  ConfigPtr path_client =
      CreateConfig(executable_dir, false, 100000, &result.error);
  ConfigPtr path_server =
      CreateConfig(executable_dir, true, 100000, &result.error);
  if (!path_client || !path_server) {
    return result;
  }
  ConnectionPair path_pair =
      CreatePair(path_client.get(), path_server.get(), 1, &result.error);
  if (!path_pair.client || !path_pair.server ||
      !ExchangeUntilEstablished(&path_pair, &result.max_pacing_delay_ns,
                                &result.error) ||
      !ExercisePathStats(&path_pair, &result, &result.error)) {
    return result;
  }

  bool accepted_established = false;
  if (!ExerciseEarlyData(executable_dir, false, 10,
                         &result.early_data_accepted_reason,
                         &accepted_established, &result.error) ||
      !accepted_established) {
    if (result.error.empty()) {
      result.error = "accepted early-data connection did not establish";
    }
    return result;
  }
  if (!ExerciseEarlyData(executable_dir, true, 20,
                         &result.early_data_rejected_reason,
                         &result.rejection_fell_back_to_1rtt, &result.error)) {
    return result;
  }
  if (result.max_pacing_delay_ns == 0 || result.rtt_ns == 0 ||
      result.min_rtt_ns == 0 || result.max_rtt_ns < result.rtt_ns ||
      result.min_rtt_ns > result.rtt_ns ||
      result.delivery_rate_bytes_per_second == 0 ||
      result.max_bandwidth_bytes_per_second == 0 || result.pmtu_bytes != 1350 ||
      result.dgram_lost == 0 ||
      result.early_data_accepted_reason != QUICHE_EARLY_DATA_ACCEPTED ||
      result.early_data_rejected_reason !=
          QUICHE_EARLY_DATA_QUIC_PARAMETER_MISMATCH ||
      !result.rejection_fell_back_to_1rtt) {
    result.error = "quiche C ABI did not meet the P05 contract";
  }
  return result;
}

}  // namespace webrtc::roq
