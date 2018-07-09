/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/packet.h"

#include <assert.h>

#include "modules/include/module_common_types.h"

namespace webrtc {

VCMPacket::VCMPacket()
    : payloadType(0),
      timestamp(0),
      ntp_time_ms_(0),
      seqNum(0),
      dataPtr(NULL),
      sizeBytes(0),
      markerBit(false),
      timesNacked(-1),
      frameType(kEmptyFrame),
      codec(kVideoCodecUnknown),
      is_first_packet_in_frame(false),
      completeNALU(kNaluUnset),
      insertStartCode(false),
      width(0),
      height(0),
      video_header(),
      receive_time_ms(0) {
  video_header.playout_delay = {-1, -1};
}

VCMPacket::VCMPacket(const uint8_t* ptr,
                     const size_t size,
                     const WebRtcRTPHeader& rtpHeader)
    : payloadType(rtpHeader.header.payloadType),
      timestamp(rtpHeader.header.timestamp),
      ntp_time_ms_(rtpHeader.ntp_time_ms),
      seqNum(rtpHeader.header.sequenceNumber),
      dataPtr(ptr),
      sizeBytes(size),
      markerBit(rtpHeader.header.markerBit),
      timesNacked(-1),
      frameType(rtpHeader.frameType),
      codec(kVideoCodecUnknown),
      is_first_packet_in_frame(
          rtpHeader.video_header().is_first_packet_in_frame),
      completeNALU(kNaluComplete),
      insertStartCode(false),
      width(rtpHeader.video_header().width),
      height(rtpHeader.video_header().height),
      video_header(rtpHeader.video_header()) {
  CopyCodecSpecifics(rtpHeader.video_header());

  if (markerBit) {
    video_header.rotation = rtpHeader.video_header().rotation;
  }
  // Playout decisions are made entirely based on first packet in a frame.
  if (is_first_packet_in_frame) {
    video_header.playout_delay = rtpHeader.video_header().playout_delay;
  } else {
    video_header.playout_delay = {-1, -1};
  }
}

void VCMPacket::Reset() {
  payloadType = 0;
  timestamp = 0;
  ntp_time_ms_ = 0;
  seqNum = 0;
  dataPtr = NULL;
  sizeBytes = 0;
  markerBit = false;
  timesNacked = -1;
  frameType = kEmptyFrame;
  codec = kVideoCodecUnknown;
  is_first_packet_in_frame = false;
  completeNALU = kNaluUnset;
  insertStartCode = false;
  width = 0;
  height = 0;
  video_header = {};
}

void VCMPacket::CopyCodecSpecifics(const RTPVideoHeader& videoHeader) {
  codec = videoHeader.codec;
  switch (videoHeader.codec) {
    case kVideoCodecVP8:
      // Handle all packets within a frame as depending on the previous packet
      // TODO(holmer): This should be changed to make fragments independent
      // when the VP8 RTP receiver supports fragments.
      if (is_first_packet_in_frame && markerBit)
        completeNALU = kNaluComplete;
      else if (is_first_packet_in_frame)
        completeNALU = kNaluStart;
      else if (markerBit)
        completeNALU = kNaluEnd;
      else
        completeNALU = kNaluIncomplete;

      return;
    case kVideoCodecVP9:
      if (is_first_packet_in_frame && markerBit)
        completeNALU = kNaluComplete;
      else if (is_first_packet_in_frame)
        completeNALU = kNaluStart;
      else if (markerBit)
        completeNALU = kNaluEnd;
      else
        completeNALU = kNaluIncomplete;

      return;
    case kVideoCodecH264:
      is_first_packet_in_frame = videoHeader.is_first_packet_in_frame;
      if (is_first_packet_in_frame)
        insertStartCode = true;

      if (is_first_packet_in_frame && markerBit) {
        completeNALU = kNaluComplete;
      } else if (is_first_packet_in_frame) {
        completeNALU = kNaluStart;
      } else if (markerBit) {
        completeNALU = kNaluEnd;
      } else {
        completeNALU = kNaluIncomplete;
      }
      return;
    case kVideoCodecGeneric:
      return;
    default:
      codec = kVideoCodecUnknown;
      return;
  }
}

}  // namespace webrtc
