/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/roq/capability.h"

#include <arpa/inet.h>
#include <sys/socket.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "api/environment/environment_factory.h"
#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_bitrate_allocation.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_type.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/codecs/h264/h264_encoder_impl.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "quiche.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

namespace webrtc::roq {
namespace {

constexpr std::array<uint8_t, 7> kAlpnWire = {6, 'r', 'o', 'q', '-', '1', '4'};
constexpr std::string_view kAlpn = "roq-14";
constexpr size_t kConnectionIdLength = 16;
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

sockaddr_in LoopbackAddress(uint16_t port) {
  sockaddr_in address = {};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  return address;
}

bool ConfigureCommon(quiche_config* config) {
  if (config == nullptr ||
      quiche_config_set_application_protos(config, kAlpnWire.data(),
                                           kAlpnWire.size()) != 0 ||
      quiche_config_set_cc_algorithm_name(config, "bbr2_gcongestion") != 0) {
    return false;
  }
  quiche_config_set_max_idle_timeout(config, 5000);
  quiche_config_set_max_recv_udp_payload_size(config, 1350);
  quiche_config_set_max_send_udp_payload_size(config, 1350);
  quiche_config_set_initial_max_data(config, 100000);
  quiche_config_set_initial_max_stream_data_bidi_local(config, 10000);
  quiche_config_set_initial_max_stream_data_bidi_remote(config, 10000);
  quiche_config_set_initial_max_streams_bidi(config, 4);
  return true;
}

bool TransferPackets(quiche_conn* sender,
                     quiche_conn* receiver,
                     std::string* error) {
  std::array<uint8_t, kPacketBufferSize> packet = {};
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

bool RunQuicHandshake(const std::string& executable_path,
                      std::string* negotiated_alpn,
                      std::string* error) {
  ConfigPtr client_config(quiche_config_new(QUICHE_PROTOCOL_VERSION));
  ConfigPtr server_config(quiche_config_new(QUICHE_PROTOCOL_VERSION));
  if (!ConfigureCommon(client_config.get()) ||
      !ConfigureCommon(server_config.get())) {
    *error = "failed to configure quiche";
    return false;
  }
  quiche_config_verify_peer(client_config.get(), false);

  const std::filesystem::path executable_dir =
      std::filesystem::absolute(executable_path).parent_path();
  const std::string cert_path = (executable_dir / "cert.crt").string();
  const std::string key_path = (executable_dir / "cert.key").string();
  if (quiche_config_load_cert_chain_from_pem_file(server_config.get(),
                                                  cert_path.c_str()) != 0 ||
      quiche_config_load_priv_key_from_pem_file(server_config.get(),
                                                key_path.c_str()) != 0) {
    *error = "failed to load test certificate from " + executable_dir.string();
    return false;
  }

  const sockaddr_in client_address = LoopbackAddress(12345);
  const sockaddr_in server_address = LoopbackAddress(4433);
  const std::array<uint8_t, kConnectionIdLength> client_scid = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

  ConnectionPtr client(
      quiche_connect("quic.tech", client_scid.data(), client_scid.size(),
                     reinterpret_cast<const sockaddr*>(&client_address),
                     sizeof(client_address),
                     reinterpret_cast<const sockaddr*>(&server_address),
                     sizeof(server_address), client_config.get()));
  if (!client) {
    *error = "failed to create quiche client";
    return false;
  }

  const uint8_t* client_dcid = nullptr;
  size_t client_dcid_length = 0;
  quiche_conn_destination_id(client.get(), &client_dcid, &client_dcid_length);

  ConnectionPtr server(
      quiche_accept(client_dcid, client_dcid_length, nullptr, 0,
                    reinterpret_cast<const sockaddr*>(&server_address),
                    sizeof(server_address),
                    reinterpret_cast<const sockaddr*>(&client_address),
                    sizeof(client_address), server_config.get()));
  if (!server) {
    *error = "failed to create quiche server";
    return false;
  }

  for (int round = 0;
       round < 100 && (!quiche_conn_is_established(client.get()) ||
                       !quiche_conn_is_established(server.get()));
       ++round) {
    if (!TransferPackets(client.get(), server.get(), error) ||
        !TransferPackets(server.get(), client.get(), error)) {
      return false;
    }
  }

  if (!quiche_conn_is_established(client.get()) ||
      !quiche_conn_is_established(server.get())) {
    *error = "QUIC handshake did not complete";
    return false;
  }

  const uint8_t* alpn_data = nullptr;
  size_t alpn_length = 0;
  quiche_conn_application_proto(client.get(), &alpn_data, &alpn_length);
  negotiated_alpn->assign(reinterpret_cast<const char*>(alpn_data),
                          alpn_length);
  if (*negotiated_alpn != kAlpn) {
    *error = "unexpected negotiated ALPN";
    return false;
  }
  return true;
}

class CountingEncodedImageCallback final : public EncodedImageCallback {
 public:
  Result OnEncodedImage(const EncodedImage& image,
                        const CodecSpecificInfo*) override {
    encoded_bytes += image.size();
    return Result(Result::OK);
  }

  void OnFrameDropped(uint32_t, int, bool) override {}

  size_t encoded_bytes = 0;
};

size_t EncodeOpenH264Frame(std::string* error) {
  constexpr int kWidth = 1920;
  constexpr int kHeight = 1080;
  H264EncoderImpl encoder(
      CreateEnvironment(),
      {.packetization_mode = H264PacketizationMode::NonInterleaved});
  VideoCodec codec;
  codec.codecType = kVideoCodecH264;
  codec.width = kWidth;
  codec.height = kHeight;
  codec.maxFramerate = 60;
  codec.startBitrate = 4000;
  codec.maxBitrate = 5000;
  codec.minBitrate = 300;
  codec.active = true;
  codec.numberOfSimulcastStreams = 0;
  codec.qpMax = 56;
  codec.SetFrameDropEnabled(true);
  *codec.H264() = VideoEncoder::GetDefaultH264Settings();

  const VideoEncoder::Settings settings(VideoEncoder::Capabilities(false), 4,
                                        1200);
  if (encoder.InitEncode(&codec, settings) != WEBRTC_VIDEO_CODEC_OK) {
    *error = "OpenH264 InitEncode failed";
    return 0;
  }

  CountingEncodedImageCallback callback;
  if (encoder.RegisterEncodeCompleteCallback(&callback) !=
      WEBRTC_VIDEO_CODEC_OK) {
    *error = "OpenH264 callback registration failed";
    return 0;
  }

  scoped_refptr<I420Buffer> buffer = I420Buffer::Create(kWidth, kHeight);
  I420Buffer::SetBlack(buffer.get());
  VideoFrame frame = VideoFrame::Builder()
                         .set_video_frame_buffer(buffer)
                         .set_timestamp_rtp(0)
                         .build();
  const std::vector<VideoFrameType> frame_types = {
      VideoFrameType::kVideoFrameKey};
  if (encoder.Encode(frame, &frame_types) != WEBRTC_VIDEO_CODEC_OK ||
      callback.encoded_bytes == 0) {
    *error = "OpenH264 produced no encoded bytes";
    return 0;
  }
  return callback.encoded_bytes;
}

}  // namespace

CapabilityResult RunCapabilityChecks(const std::string& executable_path) {
  CapabilityResult result;
  result.ffmpeg_mov_demuxer_ok = av_find_input_format("mov") != nullptr;
  result.ffmpeg_h264_decoder_ok =
      avcodec_find_decoder(AV_CODEC_ID_H264) != nullptr;
  result.ffmpeg_aac_decoder_ok =
      avcodec_find_decoder(AV_CODEC_ID_AAC) != nullptr;
  if (!result.ffmpeg_mov_demuxer_ok || !result.ffmpeg_h264_decoder_ok ||
      !result.ffmpeg_aac_decoder_ok) {
    result.error = "required FFmpeg capabilities are unavailable";
    return result;
  }
  if (!RunQuicHandshake(executable_path, &result.alpn, &result.error)) {
    return result;
  }
  result.quic_tls_ok = true;
  result.encoded_bytes = EncodeOpenH264Frame(&result.error);
  result.openh264_encode_ok = result.encoded_bytes > 0;
  return result;
}

}  // namespace webrtc::roq
