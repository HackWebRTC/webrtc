/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"

#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/logging.h"

using webrtc::RTCPUtility::PT_BYE;
using webrtc::RTCPUtility::PT_PSFB;
using webrtc::RTCPUtility::PT_RR;
using webrtc::RTCPUtility::PT_RTPFB;
using webrtc::RTCPUtility::PT_SR;

using webrtc::RTCPUtility::RTCPPacketBYE;
using webrtc::RTCPUtility::RTCPPacketPSFBFIR;
using webrtc::RTCPUtility::RTCPPacketPSFBFIRItem;
using webrtc::RTCPUtility::RTCPPacketPSFBRPSI;
using webrtc::RTCPUtility::RTCPPacketReportBlockItem;
using webrtc::RTCPUtility::RTCPPacketRR;
using webrtc::RTCPUtility::RTCPPacketRTPFBNACK;
using webrtc::RTCPUtility::RTCPPacketRTPFBNACKItem;
using webrtc::RTCPUtility::RTCPPacketSR;

namespace webrtc {
namespace rtcp {
namespace {
// Unused SSRC of media source, set to 0.
const uint32_t kUnusedMediaSourceSsrc0 = 0;

void AssignUWord8(uint8_t* buffer, uint16_t* offset, uint8_t value) {
  buffer[(*offset)++] = value;
}
void AssignUWord16(uint8_t* buffer, uint16_t* offset, uint16_t value) {
  ModuleRTPUtility::AssignUWord16ToBuffer(buffer + *offset, value);
  *offset += 2;
}
void AssignUWord24(uint8_t* buffer, uint16_t* offset, uint32_t value) {
  ModuleRTPUtility::AssignUWord24ToBuffer(buffer + *offset, value);
  *offset += 3;
}
void AssignUWord32(uint8_t* buffer, uint16_t* offset, uint32_t value) {
  ModuleRTPUtility::AssignUWord32ToBuffer(buffer + *offset, value);
  *offset += 4;
}

uint16_t BlockToHeaderLength(uint16_t length_in_bytes) {
  // Length in 32-bit words minus 1.
  assert(length_in_bytes > 0);
  assert(length_in_bytes % 4 == 0);
  return (length_in_bytes / 4) - 1;
}

// From RFC 3550, RTP: A Transport Protocol for Real-Time Applications.
//
// RTP header format.
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P| RC/FMT  |      PT       |             length            |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

void CreateHeader(uint8_t count_or_format,  // Depends on packet type.
                  uint8_t packet_type,
                  uint16_t length,
                  uint8_t* buffer,
                  uint16_t* pos) {
  const uint8_t kVersion = 2;
  AssignUWord8(buffer, pos, (kVersion << 6) + count_or_format);
  AssignUWord8(buffer, pos, packet_type);
  AssignUWord16(buffer, pos, length);
}

//  Sender report (SR) (RFC 3550).
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P|    RC   |   PT=SR=200   |             length            |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                         SSRC of sender                        |
//  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//  |              NTP timestamp, most significant word             |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |             NTP timestamp, least significant word             |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                         RTP timestamp                         |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                     sender's packet count                     |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                      sender's octet count                     |
//  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+

void CreateSenderReport(const RTCPPacketSR& sr,
                        uint16_t length,
                        uint8_t* buffer,
                        uint16_t* pos) {
  CreateHeader(sr.NumberOfReportBlocks, PT_SR, length, buffer, pos);
  AssignUWord32(buffer, pos, sr.SenderSSRC);
  AssignUWord32(buffer, pos, sr.NTPMostSignificant);
  AssignUWord32(buffer, pos, sr.NTPLeastSignificant);
  AssignUWord32(buffer, pos, sr.RTPTimestamp);
  AssignUWord32(buffer, pos, sr.SenderPacketCount);
  AssignUWord32(buffer, pos, sr.SenderOctetCount);
}

//  Receiver report (RR), header (RFC 3550).
//
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P|    RC   |   PT=RR=201   |             length            |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                     SSRC of packet sender                     |
//  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+

void CreateReceiverReport(const RTCPPacketRR& rr,
                          uint16_t length,
                          uint8_t* buffer,
                          uint16_t* pos) {
  CreateHeader(rr.NumberOfReportBlocks, PT_RR, length, buffer, pos);
  AssignUWord32(buffer, pos, rr.SenderSSRC);
}

//  Report block (RFC 3550).
//
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//  |                 SSRC_1 (SSRC of first source)                 |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  | fraction lost |       cumulative number of packets lost       |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |           extended highest sequence number received           |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                      interarrival jitter                      |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                         last SR (LSR)                         |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                   delay since last SR (DLSR)                  |
//  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+

void CreateReportBlock(const RTCPPacketReportBlockItem& report_block,
                       uint8_t* buffer,
                       uint16_t* pos) {
  AssignUWord32(buffer, pos, report_block.SSRC);
  AssignUWord8(buffer, pos, report_block.FractionLost);
  AssignUWord24(buffer, pos, report_block.CumulativeNumOfPacketsLost);
  AssignUWord32(buffer, pos, report_block.ExtendedHighestSequenceNumber);
  AssignUWord32(buffer, pos, report_block.Jitter);
  AssignUWord32(buffer, pos, report_block.LastSR);
  AssignUWord32(buffer, pos, report_block.DelayLastSR);
}

// Bye packet (BYE) (RFC 3550).
//
//        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |V=2|P|    SC   |   PT=BYE=203  |             length            |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |                           SSRC/CSRC                           |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       :                              ...                              :
//       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// (opt) |     length    |               reason for leaving            ...
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

void CreateBye(const RTCPPacketBYE& bye,
               const std::vector<uint32_t>& csrcs,
               uint16_t length,
               uint8_t* buffer,
               uint16_t* pos) {
  CreateHeader(length, PT_BYE, length, buffer, pos);
  AssignUWord32(buffer, pos, bye.SenderSSRC);
  for (std::vector<uint32_t>::const_iterator it = csrcs.begin();
       it != csrcs.end(); ++it) {
    AssignUWord32(buffer, pos, *it);
  }
}

// RFC 4585: Feedback format.
//
// Common packet format:
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |V=2|P|   FMT   |       PT      |          length               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                  SSRC of packet sender                        |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                  SSRC of media source                         |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   :            Feedback Control Information (FCI)                 :
//   :
//

// Generic NACK (RFC 4585).
//
// FCI:
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |            PID                |             BLP               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

void CreateNack(const RTCPPacketRTPFBNACK& nack,
                const std::vector<RTCPPacketRTPFBNACKItem>& nack_fields,
                uint16_t length,
                uint8_t* buffer,
                uint16_t* pos) {
  const uint8_t kFmt = 1;
  CreateHeader(kFmt, PT_RTPFB, length, buffer, pos);
  AssignUWord32(buffer, pos, nack.SenderSSRC);
  AssignUWord32(buffer, pos, nack.MediaSSRC);
  for (std::vector<RTCPPacketRTPFBNACKItem>::const_iterator
      it = nack_fields.begin(); it != nack_fields.end(); ++it) {
    AssignUWord16(buffer, pos, (*it).PacketID);
    AssignUWord16(buffer, pos, (*it).BitMask);
  }
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
                uint16_t length,
                uint8_t* buffer,
                uint16_t* pos) {
  // Native bit string should be a multiple of 8 bits.
  assert(rpsi.NumberOfValidBits % 8 == 0);
  const uint8_t kFmt = 3;
  CreateHeader(kFmt, PT_PSFB, length, buffer, pos);
  AssignUWord32(buffer, pos, rpsi.SenderSSRC);
  AssignUWord32(buffer, pos, rpsi.MediaSSRC);
  AssignUWord8(buffer, pos, padding_bytes * 8);
  AssignUWord8(buffer, pos, rpsi.PayloadType);
  memcpy(buffer + *pos, rpsi.NativeBitString, rpsi.NumberOfValidBits / 8);
  *pos += rpsi.NumberOfValidBits / 8;
  memset(buffer + *pos, 0, padding_bytes);
  *pos += padding_bytes;
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
               uint16_t length,
               uint8_t* buffer,
               uint16_t* pos) {
  const uint8_t kFmt = 4;
  CreateHeader(kFmt, PT_PSFB, length, buffer, pos);
  AssignUWord32(buffer, pos, fir.SenderSSRC);
  AssignUWord32(buffer, pos, kUnusedMediaSourceSsrc0);
  AssignUWord32(buffer, pos, fir_item.SSRC);
  AssignUWord8(buffer, pos, fir_item.CommandSequenceNumber);
  AssignUWord24(buffer, pos, 0);
}

template <typename T>
void AppendBlocks(const std::vector<T*>& blocks,
                  uint8_t* buffer,
                  uint16_t* pos) {
  for (typename std::vector<T*>::const_iterator it = blocks.begin();
       it != blocks.end(); ++it) {
    (*it)->Create(buffer, pos);
  }
}
}  // namespace

void RtcpPacket::Append(RtcpPacket* packet) {
  assert(packet);
  appended_packets_.push_back(packet);
}

RawPacket RtcpPacket::Build() const {
  uint16_t len = 0;
  uint8_t packet[IP_PACKET_SIZE];
  CreateAndAddAppended(packet, &len, IP_PACKET_SIZE);
  return RawPacket(packet, len);
}

void RtcpPacket::Build(uint8_t* packet, uint16_t* len, uint16_t max_len) const {
  *len = 0;
  CreateAndAddAppended(packet, len, max_len);
}

void RtcpPacket::CreateAndAddAppended(uint8_t* packet,
                                      uint16_t* len,
                                      uint16_t max_len) const {
  Create(packet, len, max_len);
  for (std::vector<RtcpPacket*>::const_iterator it = appended_packets_.begin();
      it != appended_packets_.end(); ++it) {
    (*it)->CreateAndAddAppended(packet, len, max_len);
  }
}

void Empty::Create(uint8_t* packet, uint16_t* len, uint16_t max_len) const {
}

void SenderReport::Create(uint8_t* packet,
                          uint16_t* len,
                          uint16_t max_len) const {
  if (*len + Length() > max_len) {
    LOG(LS_WARNING) << "Max packet size reached.";
    return;
  }
  CreateSenderReport(sr_, BlockToHeaderLength(Length()), packet, len);
  AppendBlocks(report_blocks_, packet, len);
}

void SenderReport::WithReportBlock(ReportBlock* block) {
  assert(block);
  if (report_blocks_.size() >= kMaxNumberOfReportBlocks) {
    LOG(LS_WARNING) << "Max report blocks reached.";
    return;
  }
  report_blocks_.push_back(block);
  sr_.NumberOfReportBlocks = report_blocks_.size();
}

void ReceiverReport::Create(uint8_t* packet,
                            uint16_t* len,
                            uint16_t max_len) const {
  if (*len + Length() > max_len) {
    LOG(LS_WARNING) << "Max packet size reached.";
    return;
  }
  CreateReceiverReport(rr_, BlockToHeaderLength(Length()), packet, len);
  AppendBlocks(report_blocks_, packet, len);
}

void ReceiverReport::WithReportBlock(ReportBlock* block) {
  assert(block);
  if (report_blocks_.size() >= kMaxNumberOfReportBlocks) {
    LOG(LS_WARNING) << "Max report blocks reached.";
    return;
  }
  report_blocks_.push_back(block);
  rr_.NumberOfReportBlocks = report_blocks_.size();
}

void ReportBlock::Create(uint8_t* packet, uint16_t* len) const {
  CreateReportBlock(report_block_, packet, len);
}

void Bye::Create(uint8_t* packet, uint16_t* len, uint16_t max_len) const {
  if (*len + Length() > max_len) {
    LOG(LS_WARNING) << "Max packet size reached.";
    return;
  }
  CreateBye(bye_, csrcs_, BlockToHeaderLength(Length()), packet, len);
}

void Bye::WithCsrc(uint32_t csrc) {
  if (csrcs_.size() >= kMaxNumberOfCsrcs) {
    LOG(LS_WARNING) << "Max CSRC size reached.";
    return;
  }
  csrcs_.push_back(csrc);
}

void Nack::Create(uint8_t* packet, uint16_t* len, uint16_t max_len) const {
  assert(!nack_fields_.empty());
  if (*len + Length() > max_len) {
    LOG(LS_WARNING) << "Max packet size reached.";
    return;
  }
  CreateNack(nack_, nack_fields_, BlockToHeaderLength(Length()), packet, len);
}

void Nack::WithList(const uint16_t* nack_list, int length) {
  assert(nack_list);
  assert(nack_fields_.empty());
  int i = 0;
  while (i < length) {
    uint16_t pid = nack_list[i++];
    // Bitmask specifies losses in any of the 16 packets following the pid.
    uint16_t bitmask = 0;
    while (i < length) {
      int shift = static_cast<uint16_t>(nack_list[i] - pid) - 1;
      if (shift >= 0 && shift <= 15) {
        bitmask |= (1 << shift);
        ++i;
      } else {
        break;
      }
    }
    RTCPUtility::RTCPPacketRTPFBNACKItem item;
    item.PacketID = pid;
    item.BitMask = bitmask;
    nack_fields_.push_back(item);
  }
}

void Rpsi::Create(uint8_t* packet, uint16_t* len, uint16_t max_len) const {
  assert(rpsi_.NumberOfValidBits > 0);
  if (*len + Length() > max_len) {
    LOG(LS_WARNING) << "Max packet size reached.";
    return;
  }
  CreateRpsi(rpsi_, padding_bytes_, BlockToHeaderLength(Length()), packet, len);
}

void Rpsi::WithPictureId(uint64_t picture_id) {
  const uint32_t kPidBits = 7;
  const uint64_t k7MsbZeroMask = 0x1ffffffffffffff;
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

void Fir::Create(uint8_t* packet, uint16_t* len, uint16_t max_len) const {
  if (*len + Length() > max_len) {
    LOG(LS_WARNING) << "Max packet size reached.";
    return;
  }
  CreateFir(fir_, fir_item_, BlockToHeaderLength(Length()), packet, len);
}
}  // namespace rtcp
}  // namespace webrtc
