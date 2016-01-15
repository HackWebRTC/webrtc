/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/rpsi.h"

#include "webrtc/modules/rtp_rtcp/source/byte_io.h"

using webrtc::RTCPUtility::PT_PSFB;
using webrtc::RTCPUtility::RTCPPacketPSFBRPSI;

namespace webrtc {
namespace rtcp {
namespace {
void AssignUWord8(uint8_t* buffer, size_t* offset, uint8_t value) {
  buffer[(*offset)++] = value;
}

void AssignUWord32(uint8_t* buffer, size_t* offset, uint32_t value) {
  ByteWriter<uint32_t>::WriteBigEndian(buffer + *offset, value);
  *offset += 4;
}

// Reference picture selection indication (RPSI) (RFC 4585).
//
// FCI:
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |      PB       |0| Payload Type|    Native RPSI bit string     |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |   defined per codec          ...                | Padding (0) |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
void CreateRpsi(const RTCPPacketPSFBRPSI& rpsi,
                uint8_t padding_bytes,
                uint8_t* buffer,
                size_t* pos) {
  // Native bit string should be a multiple of 8 bits.
  assert(rpsi.NumberOfValidBits % 8 == 0);
  AssignUWord32(buffer, pos, rpsi.SenderSSRC);
  AssignUWord32(buffer, pos, rpsi.MediaSSRC);
  AssignUWord8(buffer, pos, padding_bytes * 8);
  AssignUWord8(buffer, pos, rpsi.PayloadType);
  memcpy(buffer + *pos, rpsi.NativeBitString, rpsi.NumberOfValidBits / 8);
  *pos += rpsi.NumberOfValidBits / 8;
  memset(buffer + *pos, 0, padding_bytes);
  *pos += padding_bytes;
}
}  // namespace

bool Rpsi::Create(uint8_t* packet,
                  size_t* index,
                  size_t max_length,
                  RtcpPacket::PacketReadyCallback* callback) const {
  assert(rpsi_.NumberOfValidBits > 0);
  while (*index + BlockLength() > max_length) {
    if (!OnBufferFull(packet, index, callback))
      return false;
  }
  const uint8_t kFmt = 3;
  CreateHeader(kFmt, PT_PSFB, HeaderLength(), packet, index);
  CreateRpsi(rpsi_, padding_bytes_, packet, index);
  return true;
}

void Rpsi::WithPictureId(uint64_t picture_id) {
  const uint32_t kPidBits = 7;
  const uint64_t k7MsbZeroMask = 0x1ffffffffffffffULL;
  uint8_t required_bytes = 0;
  uint64_t shifted_pid = picture_id;
  do {
    ++required_bytes;
    shifted_pid = (shifted_pid >> kPidBits) & k7MsbZeroMask;
  } while (shifted_pid > 0);

  // Convert picture id to native bit string (natively defined by the video
  // codec).
  int pos = 0;
  for (int i = required_bytes - 1; i > 0; i--) {
    rpsi_.NativeBitString[pos++] =
        0x80 | static_cast<uint8_t>(picture_id >> (i * kPidBits));
  }
  rpsi_.NativeBitString[pos++] = static_cast<uint8_t>(picture_id & 0x7f);
  rpsi_.NumberOfValidBits = pos * 8;

  // Calculate padding bytes (to reach next 32-bit boundary, 1, 2 or 3 bytes).
  padding_bytes_ = 4 - ((2 + required_bytes) % 4);
  if (padding_bytes_ == 4) {
    padding_bytes_ = 0;
  }
}


}  // namespace rtcp
}  // namespace webrtc
