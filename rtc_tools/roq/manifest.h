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
  std::optional<uint32_t> payload_type;
  std::optional<uint32_t> ssrc;
};

struct ManifestMappingResult {
  uint64_t flow_id = 0;
  uint64_t generation = 0;
  std::string config_hash;
  std::string error;
};

ManifestMappingResult LoadAndMapManifest(const std::string& path,
                                         const PacketMetadata& packet);

}  // namespace webrtc::roq

#endif  // RTC_TOOLS_ROQ_MANIFEST_H_
