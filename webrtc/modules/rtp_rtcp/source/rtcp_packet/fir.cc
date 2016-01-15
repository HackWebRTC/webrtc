/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/fir.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"

using webrtc::RTCPUtility::PT_PSFB;
using webrtc::RTCPUtility::RTCPPacketPSFBFIR;
using webrtc::RTCPUtility::RTCPPacketPSFBFIRItem;

namespace webrtc {
namespace rtcp {
namespace {
const uint32_t kUnusedMediaSourceSsrc0 = 0;

void AssignUWord8(uint8_t* buffer, size_t* offset, uint8_t value) {
  buffer[(*offset)++] = value;
}

void AssignUWord24(uint8_t* buffer, size_t* offset, uint32_t value) {
  ByteWriter<uint32_t, 3>::WriteBigEndian(buffer + *offset, value);
  *offset += 3;
}

void AssignUWord32(uint8_t* buffer, size_t* offset, uint32_t value) {
  ByteWriter<uint32_t>::WriteBigEndian(buffer + *offset, value);
  *offset += 4;
}

// Full intra request (FIR) (RFC 5104).
//
// FCI:
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                              SSRC                             |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   | Seq nr.       |    Reserved                                   |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
void CreateFir(const RTCPPacketPSFBFIR& fir,
               const RTCPPacketPSFBFIRItem& fir_item,
               uint8_t* buffer,
               size_t* pos) {
  AssignUWord32(buffer, pos, fir.SenderSSRC);
  AssignUWord32(buffer, pos, kUnusedMediaSourceSsrc0);
  AssignUWord32(buffer, pos, fir_item.SSRC);
  AssignUWord8(buffer, pos, fir_item.CommandSequenceNumber);
  AssignUWord24(buffer, pos, 0);
}
}  // namespace

bool Fir::Create(uint8_t* packet,
                 size_t* index,
                 size_t max_length,
                 RtcpPacket::PacketReadyCallback* callback) const {
  while (*index + BlockLength() > max_length) {
    if (!OnBufferFull(packet, index, callback))
      return false;
  }
  const uint8_t kFmt = 4;
  CreateHeader(kFmt, PT_PSFB, HeaderLength(), packet, index);
  CreateFir(fir_, fir_item_, packet, index);
  return true;
}

}  // namespace rtcp
}  // namespace webrtc
