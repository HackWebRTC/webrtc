/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_ROQ_MANIFEST_H_
#define RTC_TOOLS_ROQ_MANIFEST_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace webrtc::roq {

struct PacketMetadata {
  std::string kind;
  std::string role;
  std::optional<uint32_t> payload_type;
  std::optional<uint32_t> ssrc;
};

struct ManifestMedia {
  std::string codec;
  uint32_t payload_type = 0;
  uint32_t flow_id = 0;
  uint32_t client_ssrc = 0;
  uint32_t server_ssrc = 0;
};

struct SessionManifest {
  uint32_t generation = 0;
  std::string alpn;
  std::string config_hash;
  ManifestMedia audio;
  ManifestMedia video;
  uint32_t rtcp_flow_id = 0;
};

struct ManifestMappingResult {
  uint64_t flow_id = 0;
  uint64_t generation = 0;
  uint32_t sender_ssrc = 0;
  std::string config_hash;
  std::string error;
};

ManifestMappingResult LoadAndMapManifest(const std::string& path,
                                         const PacketMetadata& packet);

bool LoadSessionManifest(const std::string& path,
                         SessionManifest* manifest,
                         std::string* error);

}  // namespace webrtc::roq

#endif  // RTC_TOOLS_ROQ_MANIFEST_H_
