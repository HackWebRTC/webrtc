/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_depacketizer_av1.h"

#include <stddef.h>
#include <stdint.h>

#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "rtc_base/byte_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {
// AV1 format:
//
// RTP payload syntax:
//     0 1 2 3 4 5 6 7
//    +-+-+-+-+-+-+-+-+
//    |Z|Y| W |-|-|-|-| (REQUIRED)
//    +=+=+=+=+=+=+=+=+ (REPEATED W-1 times, or any times if W = 0)
//    |1|             |
//    +-+ OBU fragment|
//    |1|             | (REQUIRED, leb128 encoded)
//    +-+    size     |
//    |0|             |
//    +-+-+-+-+-+-+-+-+
//    |  OBU fragment |
//    |     ...       |
//    +=+=+=+=+=+=+=+=+
//    |     ...       |
//    +=+=+=+=+=+=+=+=+ if W > 0, last fragment MUST NOT have size field
//    |  OBU fragment |
//    |     ...       |
//    +=+=+=+=+=+=+=+=+
//
//
// OBU syntax:
//     0 1 2 3 4 5 6 7
//    +-+-+-+-+-+-+-+-+
//    |0| type  |X|S|-| (REQUIRED)
//    +-+-+-+-+-+-+-+-+
// X: | TID |SID|-|-|-| (OPTIONAL)
//    +-+-+-+-+-+-+-+-+
//    |1|             |
//    +-+ OBU payload |
// S: |1|             | (OPTIONAL, variable length leb128 encoded)
//    +-+    size     |
//    |0|             |
//    +-+-+-+-+-+-+-+-+
//    |  OBU payload  |
//    |     ...       |
constexpr int kObuTypeSequenceHeader = 1;

int ObuType(uint8_t obu_header) {
  return (obu_header & 0b0'1111'000u) >> 3;
}

bool RtpStartsWithFragment(uint8_t aggregation_header) {
  return aggregation_header & 0b1000'0000u;
}
bool RtpEndsWithFragment(uint8_t aggregation_header) {
  return aggregation_header & 0b0100'0000u;
}
int RtpNumObus(uint8_t aggregation_header) {  // 0 for any number of obus.
  return (aggregation_header & 0b0011'0000u) >> 4;
}

}  // namespace

bool RtpDepacketizerAv1::Parse(ParsedPayload* parsed_payload,
                               const uint8_t* payload_data,
                               size_t payload_data_length) {
  RTC_DCHECK(parsed_payload);
  if (payload_data_length == 0) {
    RTC_DLOG(LS_ERROR) << "Empty rtp payload.";
    return false;
  }
  // To assemble frame, all of the rtp payload is required, including
  // aggregation header.
  parsed_payload->payload = payload_data;
  parsed_payload->payload_length = payload_data_length;

  rtc::ByteBufferReader payload(reinterpret_cast<const char*>(payload_data),
                                payload_data_length);
  uint8_t aggregation_header;
  RTC_CHECK(payload.ReadUInt8(&aggregation_header));

  // TODO(danilchap): Set AV1 codec when there is such enum value
  parsed_payload->video.codec = VideoCodecType::kVideoCodecGeneric;
  // These are not accurate since frame may consist of several packet aligned
  // chunks of obus, but should be good enough for most cases. It might produce
  // frame that do not map to any real frame, but av1 decoder should be able to
  // handle it since it promise to handle individual obus rather than full
  // frames.
  parsed_payload->video.is_first_packet_in_frame =
      !RtpStartsWithFragment(aggregation_header);
  parsed_payload->video.is_last_packet_in_frame =
      !RtpEndsWithFragment(aggregation_header);
  parsed_payload->video.frame_type = VideoFrameType::kVideoFrameDelta;
  // If packet starts a frame, check if it contains Sequence Header OBU.
  // In that case treat it as key frame packet.
  if (parsed_payload->video.is_first_packet_in_frame) {
    int num_expected_obus = RtpNumObus(aggregation_header);

    // The only OBU that can preceed SequenceHeader is a TemporalDelimiter OBU,
    // so check no more than two OBUs while searching for SH.
    for (int obu_index = 1; payload.Length() > 0 && obu_index <= 2;
         ++obu_index) {
      uint64_t fragment_size;
      // When num_expected_obus > 0, last OBU (fragment) is not preceeded by
      // the size field. See W field in
      // https://aomediacodec.github.io/av1-rtp-spec/#43-av1-aggregation-header
      bool has_fragment_size = (obu_index != num_expected_obus);
      if (has_fragment_size) {
        if (!payload.ReadUVarint(&fragment_size)) {
          RTC_DLOG(LS_WARNING)
              << "Failed to read OBU fragment size for OBU#" << obu_index;
          return false;
        }
        if (fragment_size > payload.Length()) {
          RTC_DLOG(LS_WARNING) << "OBU fragment size " << fragment_size
                               << " exceeds remaining payload size "
                               << payload.Length() << " for OBU#" << obu_index;
          // Malformed input: written size is larger than remaining buffer.
          return false;
        }
      } else {
        fragment_size = payload.Length();
      }
      // Though it is inpractical to pass empty fragments, it is allowed.
      if (fragment_size == 0) {
        RTC_LOG(LS_WARNING)
            << "Weird obu of size 0 at offset "
            << (payload_data_length - payload.Length()) << ", skipping.";
        continue;
      }
      uint8_t obu_header = *reinterpret_cast<const uint8_t*>(payload.Data());
      if (ObuType(obu_header) == kObuTypeSequenceHeader) {
        // TODO(bugs.webrtc.org/11042): Check frame_header OBU and/or frame OBU
        // too for other conditions of the start of a new coded video sequence.
        // For proper checks checking single packet might not be enough. See
        // https://aomediacodec.github.io/av1-spec/av1-spec.pdf section 7.5
        parsed_payload->video.frame_type = VideoFrameType::kVideoFrameKey;
        break;
      }
      payload.Consume(fragment_size);
    }
  }

  return true;
}

}  // namespace webrtc
