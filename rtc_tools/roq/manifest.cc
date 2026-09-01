/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/roq/manifest.h"

#include <openssl/sha.h>

#include <array>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

#include "json/reader.h"
#include "json/value.h"

namespace webrtc::roq {
namespace {

std::string Sha256(const std::string& bytes) {
  std::array<uint8_t, SHA256_DIGEST_LENGTH> digest = {};
  SHA256(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(),
         digest.data());
  std::ostringstream output;
  for (uint8_t byte : digest) {
    output << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(byte);
  }
  return output.str();
}

bool GetRequiredUInt(const Json::Value& object,
                     const char* name,
                     uint32_t* value) {
  const Json::Value& field = object[name];
  if (!field.isUInt()) {
    return false;
  }
  *value = field.asUInt();
  return true;
}

bool GetRequiredString(const Json::Value& object,
                       const char* name,
                       std::string* value) {
  const Json::Value& field = object[name];
  if (!field.isString()) {
    return false;
  }
  *value = field.asString();
  return true;
}

bool ParseStream(const Json::Value& root,
                 const char* name,
                 const char* expected_codec,
                 uint32_t* payload_type,
                 uint32_t* client_ssrc,
                 uint32_t* server_ssrc,
                 uint32_t* flow_id) {
  const Json::Value& stream = root[name];
  const Json::Value& senders = stream["senders"];
  std::string codec;
  return stream.isObject() && GetRequiredString(stream, "codec", &codec) &&
         codec == expected_codec &&
         GetRequiredUInt(stream, "payload_type", payload_type) &&
         senders.isObject() && senders["client"].isObject() &&
         senders["server"].isObject() &&
         GetRequiredUInt(senders["client"], "ssrc", client_ssrc) &&
         GetRequiredUInt(senders["server"], "ssrc", server_ssrc) &&
         *client_ssrc != *server_ssrc &&
         GetRequiredUInt(stream, "flow_id", flow_id) && *flow_id < 64;
}

}  // namespace

bool LoadSessionManifest(const std::string& path,
                         SessionManifest* manifest,
                         std::string* error) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    *error = "failed to open manifest";
    return false;
  }
  const std::string raw((std::istreambuf_iterator<char>(input)),
                        std::istreambuf_iterator<char>());

  Json::CharReaderBuilder builder;
  Json::Value root;
  std::string parse_error;
  std::istringstream json_input(raw);
  if (!Json::parseFromStream(builder, json_input, &root, &parse_error) ||
      !root.isObject()) {
    *error = "invalid manifest JSON: " + parse_error;
    return false;
  }

  uint32_t schema_version = 0;
  if (!GetRequiredUInt(root, "schema_version", &schema_version) ||
      schema_version != 2) {
    *error = "unsupported schema_version";
    return false;
  }
  uint32_t roq_version = 0;
  if (!GetRequiredUInt(root, "roq_version", &roq_version) ||
      roq_version != 14) {
    *error = "unsupported roq_version";
    return false;
  }
  std::string alpn;
  if (!GetRequiredString(root, "alpn", &alpn) || alpn != "roq-14") {
    *error = "unsupported ALPN";
    return false;
  }
  uint32_t generation = 0;
  if (!GetRequiredUInt(root, "generation", &generation) || generation == 0) {
    *error = "invalid generation";
    return false;
  }

  uint32_t audio_payload_type = 0;
  uint32_t audio_ssrc = 0;
  uint32_t server_audio_ssrc = 0;
  uint32_t audio_flow_id = 0;
  uint32_t video_payload_type = 0;
  uint32_t video_ssrc = 0;
  uint32_t server_video_ssrc = 0;
  uint32_t video_flow_id = 0;
  uint32_t rtcp_flow_id = 0;
  std::string video_profile;
  uint32_t packetization_mode = 0;
  if (!ParseStream(root, "audio", "opus", &audio_payload_type, &audio_ssrc,
                   &server_audio_ssrc, &audio_flow_id) ||
      !ParseStream(root, "video", "H264", &video_payload_type, &video_ssrc,
                   &server_video_ssrc, &video_flow_id) ||
      !root["rtcp"].isObject() ||
      !GetRequiredUInt(root["rtcp"], "flow_id", &rtcp_flow_id) ||
      rtcp_flow_id >= 64 || !root["header_extensions"].isArray() ||
      !root["header_extensions"].empty()) {
    *error = "invalid manifest media mapping";
    return false;
  }
  if (!GetRequiredString(root["video"], "profile", &video_profile) ||
      video_profile != "constrained-baseline" ||
      !GetRequiredUInt(root["video"], "packetization_mode",
                       &packetization_mode) ||
      packetization_mode != 1) {
    *error = "unsupported H264 contract";
    return false;
  }
  if (audio_flow_id == video_flow_id || audio_flow_id == rtcp_flow_id ||
      video_flow_id == rtcp_flow_id) {
    *error = "flow IDs must be distinct";
    return false;
  }

  manifest->generation = generation;
  manifest->alpn = alpn;
  manifest->config_hash = Sha256(raw);
  manifest->audio = {.codec = "opus",
                     .payload_type = audio_payload_type,
                     .flow_id = audio_flow_id,
                     .client_ssrc = audio_ssrc,
                     .server_ssrc = server_audio_ssrc};
  manifest->video = {.codec = "H264",
                     .payload_type = video_payload_type,
                     .flow_id = video_flow_id,
                     .client_ssrc = video_ssrc,
                     .server_ssrc = server_video_ssrc};
  manifest->rtcp_flow_id = rtcp_flow_id;
  return true;
}

ManifestMappingResult LoadAndMapManifest(const std::string& path,
                                         const PacketMetadata& packet) {
  ManifestMappingResult result;
  SessionManifest manifest;
  if (!LoadSessionManifest(path, &manifest, &result.error)) {
    return result;
  }
  const bool is_client = packet.role == "client";
  if (packet.kind != "rtcp" && !is_client && packet.role != "server") {
    result.error = "RTP mapping requires --role <client|server>";
    return result;
  }
  const auto sender_ssrc = [is_client](const ManifestMedia& media) {
    return is_client ? media.client_ssrc : media.server_ssrc;
  };

  if (packet.kind == "audio-rtp") {
    result.sender_ssrc = sender_ssrc(manifest.audio);
    if (packet.payload_type != manifest.audio.payload_type ||
        packet.ssrc != result.sender_ssrc) {
      result.error = "audio RTP metadata does not match manifest";
      return result;
    }
    result.flow_id = manifest.audio.flow_id;
  } else if (packet.kind == "video-rtp") {
    result.sender_ssrc = sender_ssrc(manifest.video);
    if (packet.payload_type != manifest.video.payload_type ||
        packet.ssrc != result.sender_ssrc) {
      result.error = "video RTP metadata does not match manifest";
      return result;
    }
    result.flow_id = manifest.video.flow_id;
  } else if (packet.kind == "rtcp") {
    result.flow_id = manifest.rtcp_flow_id;
  } else {
    result.error = "unsupported packet kind";
    return result;
  }

  result.generation = manifest.generation;
  result.config_hash = manifest.config_hash;
  return result;
}

}  // namespace webrtc::roq
