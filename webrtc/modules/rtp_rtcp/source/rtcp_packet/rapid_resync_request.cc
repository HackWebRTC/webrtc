/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/rapid_resync_request.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

using webrtc::RTCPUtility::RtcpCommonHeader;

namespace webrtc {
namespace rtcp {
// RFC 4585: Feedback format.
// Rapid Resynchronisation Request (draft-perkins-avt-rapid-rtp-sync-03).
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P|  FMT=5  |     PT=205    |         length=2              |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                  SSRC of packet sender                        |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                  SSRC of media source                         |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
bool RapidResyncRequest::Parse(const RtcpCommonHeader& header,
                               const uint8_t* payload) {
  RTC_CHECK(header.packet_type == kPacketType);
  RTC_CHECK(header.count_or_format == kFeedbackMessageType);

  if (header.payload_size_bytes != kCommonFeedbackLength) {
    LOG(LS_WARNING) << "Packet payload size should be " << kCommonFeedbackLength
                    << " instead of " << header.payload_size_bytes
                    << " to be a valid Rapid Resynchronisation Request";
    return false;
  }

  ParseCommonFeedback(payload);
  return true;
}

bool RapidResyncRequest::Create(
    uint8_t* packet,
    size_t* index,
    size_t max_length,
    RtcpPacket::PacketReadyCallback* callback) const {
  while (*index + BlockLength() > max_length) {
    if (!OnBufferFull(packet, index, callback))
      return false;
  }

  CreateHeader(kFeedbackMessageType, kPacketType, HeaderLength(), packet,
               index);
  CreateCommonFeedback(packet + *index);
  *index += kCommonFeedbackLength;
  return true;
}
}  // namespace rtcp
}  // namespace webrtc
