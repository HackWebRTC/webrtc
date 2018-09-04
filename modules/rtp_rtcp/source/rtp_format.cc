/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_format.h"

#include <utility>

#include "absl/memory/memory.h"
#include "modules/rtp_rtcp/source/rtp_format_h264.h"
#include "modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "modules/rtp_rtcp/source/rtp_format_vp8.h"
#include "modules/rtp_rtcp/source/rtp_format_vp9.h"
#include "rtc_base/checks.h"

namespace webrtc {

std::unique_ptr<RtpPacketizer> RtpPacketizer::Create(
    VideoCodecType type,
    rtc::ArrayView<const uint8_t> payload,
    PayloadSizeLimits limits,
    // Codec-specific details.
    const RTPVideoHeader& rtp_video_header,
    FrameType frame_type,
    const RTPFragmentationHeader* fragmentation) {
  switch (type) {
    case kVideoCodecH264: {
      const auto& h264 =
          absl::get<RTPVideoHeaderH264>(rtp_video_header.video_type_header);
      auto packetizer = absl::make_unique<RtpPacketizerH264>(
          limits.max_payload_len, limits.last_packet_reduction_len,
          h264.packetization_mode);
      packetizer->SetPayloadData(payload.data(), payload.size(), fragmentation);
      return std::move(packetizer);
    }
    case kVideoCodecVP8: {
      const auto& vp8 =
          absl::get<RTPVideoHeaderVP8>(rtp_video_header.video_type_header);
      return absl::make_unique<RtpPacketizerVp8>(payload, limits, vp8);
    }
    case kVideoCodecVP9: {
      const auto& vp9 =
          absl::get<RTPVideoHeaderVP9>(rtp_video_header.video_type_header);
      auto packetizer = absl::make_unique<RtpPacketizerVp9>(
          vp9, limits.max_payload_len, limits.last_packet_reduction_len);
      packetizer->SetPayloadData(payload.data(), payload.size(), nullptr);
      return std::move(packetizer);
    }
    default: {
      auto packetizer = absl::make_unique<RtpPacketizerGeneric>(
          rtp_video_header, frame_type, limits.max_payload_len,
          limits.last_packet_reduction_len);
      packetizer->SetPayloadData(payload.data(), payload.size(), nullptr);
      return std::move(packetizer);
    }
  }
}

std::vector<size_t> RtpPacketizer::SplitAboutEqually(
    size_t payload_len,
    const PayloadSizeLimits& limits) {
  RTC_CHECK_GT(limits.max_payload_len, limits.last_packet_reduction_len);

  // Last packet can be smaller. Pretend that it's the same size, but we must
  // write more payload to it.
  size_t total_bytes = payload_len + limits.last_packet_reduction_len;
  // Integer divisions with rounding up.
  size_t num_packets_left =
      (total_bytes + limits.max_payload_len - 1) / limits.max_payload_len;
  size_t bytes_per_packet = total_bytes / num_packets_left;
  size_t num_larger_packets = total_bytes % num_packets_left;
  size_t remaining_data = payload_len;

  std::vector<size_t> result;
  result.reserve(num_packets_left);
  while (remaining_data > 0) {
    // Last num_larger_packets are 1 byte wider than the rest. Increase
    // per-packet payload size when needed.
    if (num_packets_left == num_larger_packets)
      ++bytes_per_packet;
    size_t current_packet_bytes = bytes_per_packet;
    if (current_packet_bytes > remaining_data) {
      current_packet_bytes = remaining_data;
    }
    // This is not the last packet in the whole payload, but there's no data
    // left for the last packet. Leave at least one byte for the last packet.
    if (num_packets_left == 2 && current_packet_bytes == remaining_data) {
      --current_packet_bytes;
    }

    result.push_back(current_packet_bytes);

    remaining_data -= current_packet_bytes;
    --num_packets_left;
  }

  return result;
}

RtpDepacketizer* RtpDepacketizer::Create(VideoCodecType type) {
  switch (type) {
    case kVideoCodecH264:
      return new RtpDepacketizerH264();
    case kVideoCodecVP8:
      return new RtpDepacketizerVp8();
    case kVideoCodecVP9:
      return new RtpDepacketizerVp9();
    default:
      return new RtpDepacketizerGeneric();
  }
}
}  // namespace webrtc
