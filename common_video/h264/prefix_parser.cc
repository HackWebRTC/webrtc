/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/h264/prefix_parser.h"

#include <cstdint>
#include <vector>

#include "common_video/h264/h264_common.h"
#include "rtc_base/bitstream_reader.h"

namespace {
typedef absl::optional<webrtc::PrefixParser::PrefixState> OptionalPrefix;

#define RETURN_EMPTY_ON_FAIL(x) \
  if (!(x)) {                   \
    return OptionalPrefix();       \
  }
}  // namespace

namespace webrtc {

PrefixParser::PrefixState::PrefixState() = default;
PrefixParser::PrefixState::PrefixState(const PrefixState&) = default;
PrefixParser::PrefixState::~PrefixState() = default;

// General note: this is based off the 02/2016 version of the H.264 standard.
// You can find it on this page:
// http://www.itu.int/rec/T-REC-H.264

// Unpack RBSP and parse SVC extension state from the supplied buffer.
absl::optional<PrefixParser::PrefixState> PrefixParser::ParsePrefix(
    const uint8_t* data,
    size_t length) {
  std::vector<uint8_t> unpacked_buffer = H264::ParseRbsp(data, length);
    BitstreamReader reader(unpacked_buffer);
  return ParsePrefixUpToSvcExtension(reader);
}

absl::optional<PrefixParser::PrefixState>
PrefixParser::ParsePrefixUpToSvcExtension(BitstreamReader& reader) {
  // Now, we need to use a bit buffer to parse through the actual SVC extension
  // format. See Section 7.3.1 ("NAL unit syntax") and 7.3.1.1 ("NAL unit header
  // SVC extension syntax") of the H.264 standard for a complete description.
  PrefixState svc_extension;

  // Make sure the svc_extension_flag is on.
  bool svc_extension_flag = reader.ReadBit();
  if (!svc_extension_flag)
    return OptionalPrefix();

  // idr_flag: u(1)
  svc_extension.idr_flag = reader.Read<bool>();
  // priority_id: u(6)
  svc_extension.priority_id = reader.ReadBits(6);
  // no_inter_layer_pred_flag: u(1)
  svc_extension.no_inter_layer_pred_flag = reader.Read<bool>();
  // dependency_id: u(3)
  svc_extension.dependency_id = reader.ReadBits(3);
  // quality_id: u(4)
  svc_extension.quality_id = reader.ReadBits(4);
  // temporal_id: u(3)
  svc_extension.temporal_id = reader.ReadBits(3);
  // use_ref_base_pic_flag: u(1)
  svc_extension.use_ref_base_pic_flag = reader.Read<bool>();
  // discardable_flag: u(1)
  svc_extension.discardable_flag = reader.Read<bool>();
  // output_flag: u(1)
  svc_extension.output_flag = reader.Read<bool>();

  return OptionalPrefix(svc_extension);
}

}  // namespace webrtc
