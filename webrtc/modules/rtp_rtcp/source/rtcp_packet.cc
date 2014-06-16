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

using webrtc::RTCPUtility::PT_APP;
using webrtc::RTCPUtility::PT_BYE;
using webrtc::RTCPUtility::PT_IJ;
using webrtc::RTCPUtility::PT_PSFB;
using webrtc::RTCPUtility::PT_RR;
using webrtc::RTCPUtility::PT_RTPFB;
using webrtc::RTCPUtility::PT_SDES;
using webrtc::RTCPUtility::PT_SR;
using webrtc::RTCPUtility::PT_XR;

using webrtc::RTCPUtility::RTCPPacketAPP;
using webrtc::RTCPUtility::RTCPPacketBYE;
using webrtc::RTCPUtility::RTCPPacketPSFBAPP;
using webrtc::RTCPUtility::RTCPPacketPSFBFIR;
using webrtc::RTCPUtility::RTCPPacketPSFBFIRItem;
using webrtc::RTCPUtility::RTCPPacketPSFBPLI;
using webrtc::RTCPUtility::RTCPPacketPSFBREMBItem;
using webrtc::RTCPUtility::RTCPPacketPSFBRPSI;
using webrtc::RTCPUtility::RTCPPacketPSFBSLI;
using webrtc::RTCPUtility::RTCPPacketPSFBSLIItem;
using webrtc::RTCPUtility::RTCPPacketReportBlockItem;
using webrtc::RTCPUtility::RTCPPacketRR;
using webrtc::RTCPUtility::RTCPPacketRTPFBNACK;
using webrtc::RTCPUtility::RTCPPacketRTPFBNACKItem;
using webrtc::RTCPUtility::RTCPPacketRTPFBTMMBN;
using webrtc::RTCPUtility::RTCPPacketRTPFBTMMBNItem;
using webrtc::RTCPUtility::RTCPPacketRTPFBTMMBR;
using webrtc::RTCPUtility::RTCPPacketRTPFBTMMBRItem;
using webrtc::RTCPUtility::RTCPPacketSR;
using webrtc::RTCPUtility::RTCPPacketXRDLRRReportBlockItem;
using webrtc::RTCPUtility::RTCPPacketXRReceiverReferenceTimeItem;
using webrtc::RTCPUtility::RTCPPacketXR;
using webrtc::RTCPUtility::RTCPPacketXRVOIPMetricItem;

namespace webrtc {
namespace rtcp {
namespace {
// Unused SSRC of media source, set to 0.
const uint32_t kUnusedMediaSourceSsrc0 = 0;

void AssignUWord8(uint8_t* buffer, size_t* offset, uint8_t value) {
  buffer[(*offset)++] = value;
}
void AssignUWord16(uint8_t* buffer, size_t* offset, uint16_t value) {
  ModuleRTPUtility::AssignUWord16ToBuffer(buffer + *offset, value);
  *offset += 2;
}
void AssignUWord24(uint8_t* buffer, size_t* offset, uint32_t value) {
  ModuleRTPUtility::AssignUWord24ToBuffer(buffer + *offset, value);
  *offset += 3;
}
void AssignUWord32(uint8_t* buffer, size_t* offset, uint32_t value) {
  ModuleRTPUtility::AssignUWord32ToBuffer(buffer + *offset, value);
  *offset += 4;
}

size_t BlockToHeaderLength(size_t length_in_bytes) {
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
                  size_t length,
                  uint8_t* buffer,
                  size_t* pos) {
  assert(length <= 0xffff);
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
                        size_t length,
                        uint8_t* buffer,
                        size_t* pos) {
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
                          size_t length,
                          uint8_t* buffer,
                          size_t* pos) {
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

void CreateReportBlocks(const std::vector<RTCPPacketReportBlockItem>& blocks,
                        uint8_t* buffer,
                        size_t* pos) {
  for (std::vector<RTCPPacketReportBlockItem>::const_iterator
       it = blocks.begin(); it != blocks.end(); ++it) {
    AssignUWord32(buffer, pos, (*it).SSRC);
    AssignUWord8(buffer, pos, (*it).FractionLost);
    AssignUWord24(buffer, pos, (*it).CumulativeNumOfPacketsLost);
    AssignUWord32(buffer, pos, (*it).ExtendedHighestSequenceNumber);
    AssignUWord32(buffer, pos, (*it).Jitter);
    AssignUWord32(buffer, pos, (*it).LastSR);
    AssignUWord32(buffer, pos, (*it).DelayLastSR);
  }
}

// Transmission Time Offsets in RTP Streams (RFC 5450).
//
//      0                   1                   2                   3
//      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// hdr |V=2|P|    RC   |   PT=IJ=195   |             length            |
//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//     |                      inter-arrival jitter                     |
//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//     .                                                               .
//     .                                                               .
//     .                                                               .
//     |                      inter-arrival jitter                     |
//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

void CreateIj(const std::vector<uint32_t>& ij_items,
              uint8_t* buffer,
              size_t* pos) {
  size_t length = ij_items.size();
  CreateHeader(length, PT_IJ, length, buffer, pos);
  for (std::vector<uint32_t>::const_iterator it = ij_items.begin();
       it != ij_items.end(); ++it) {
    AssignUWord32(buffer, pos, *it);
  }
}

// Source Description (SDES) (RFC 3550).
//
//         0                   1                   2                   3
//         0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// header |V=2|P|    SC   |  PT=SDES=202  |             length            |
//        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// chunk  |                          SSRC/CSRC_1                          |
//   1    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//        |                           SDES items                          |
//        |                              ...                              |
//        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// chunk  |                          SSRC/CSRC_2                          |
//   2    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//        |                           SDES items                          |
//        |                              ...                              |
//        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//
// Canonical End-Point Identifier SDES Item (CNAME)
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |    CNAME=1    |     length    | user and domain name        ...
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

void CreateSdes(const std::vector<Sdes::Chunk>& chunks,
                size_t length,
                uint8_t* buffer,
                size_t* pos) {
  CreateHeader(chunks.size(), PT_SDES, length, buffer, pos);
  const uint8_t kSdesItemType = 1;
  for (std::vector<Sdes::Chunk>::const_iterator it = chunks.begin();
       it != chunks.end(); ++it) {
    AssignUWord32(buffer, pos, (*it).ssrc);
    AssignUWord8(buffer, pos, kSdesItemType);
    AssignUWord8(buffer, pos, (*it).name.length());
    memcpy(buffer + *pos, (*it).name.data(), (*it).name.length());
    *pos += (*it).name.length();
    memset(buffer + *pos, 0, (*it).null_octets);
    *pos += (*it).null_octets;
  }
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
               size_t length,
               uint8_t* buffer,
               size_t* pos) {
  CreateHeader(length, PT_BYE, length, buffer, pos);
  AssignUWord32(buffer, pos, bye.SenderSSRC);
  for (std::vector<uint32_t>::const_iterator it = csrcs.begin();
       it != csrcs.end(); ++it) {
    AssignUWord32(buffer, pos, *it);
  }
}

// Application-Defined packet (APP) (RFC 3550).
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P| subtype |   PT=APP=204  |             length            |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                           SSRC/CSRC                           |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                          name (ASCII)                         |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                   application-dependent data                ...
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

void CreateApp(const RTCPPacketAPP& app,
               uint32_t ssrc,
               size_t length,
               uint8_t* buffer,
               size_t* pos) {
  CreateHeader(app.SubType, PT_APP, length, buffer, pos);
  AssignUWord32(buffer, pos, ssrc);
  AssignUWord32(buffer, pos, app.Name);
  memcpy(buffer + *pos, app.Data, app.Size);
  *pos += app.Size;
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

// Picture loss indication (PLI) (RFC 4585).
//
// FCI: no feedback control information.

void CreatePli(const RTCPPacketPSFBPLI& pli,
               size_t length,
               uint8_t* buffer,
               size_t* pos) {
  const uint8_t kFmt = 1;
  CreateHeader(kFmt, PT_PSFB, length, buffer, pos);
  AssignUWord32(buffer, pos, pli.SenderSSRC);
  AssignUWord32(buffer, pos, pli.MediaSSRC);
}

// Slice loss indication (SLI) (RFC 4585).
//
// FCI:
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |            First        |        Number           | PictureID |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

void CreateSli(const RTCPPacketPSFBSLI& sli,
               const RTCPPacketPSFBSLIItem& sli_item,
               size_t length,
               uint8_t* buffer,
               size_t* pos) {
  const uint8_t kFmt = 2;
  CreateHeader(kFmt, PT_PSFB, length, buffer, pos);
  AssignUWord32(buffer, pos, sli.SenderSSRC);
  AssignUWord32(buffer, pos, sli.MediaSSRC);

  AssignUWord8(buffer, pos, sli_item.FirstMB >> 5);
  AssignUWord8(buffer, pos, (sli_item.FirstMB << 3) +
                            ((sli_item.NumberOfMB >> 10) & 0x07));
  AssignUWord8(buffer, pos, sli_item.NumberOfMB >> 2);
  AssignUWord8(buffer, pos, (sli_item.NumberOfMB << 6) + sli_item.PictureId);
}

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
                size_t length,
                uint8_t* buffer,
                size_t* pos) {
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
                size_t length,
                uint8_t* buffer,
                size_t* pos) {
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
               size_t length,
               uint8_t* buffer,
               size_t* pos) {
  const uint8_t kFmt = 4;
  CreateHeader(kFmt, PT_PSFB, length, buffer, pos);
  AssignUWord32(buffer, pos, fir.SenderSSRC);
  AssignUWord32(buffer, pos, kUnusedMediaSourceSsrc0);
  AssignUWord32(buffer, pos, fir_item.SSRC);
  AssignUWord8(buffer, pos, fir_item.CommandSequenceNumber);
  AssignUWord24(buffer, pos, 0);
}
}  // namespace

void RtcpPacket::Append(RtcpPacket* packet) {
  assert(packet);
  appended_packets_.push_back(packet);
}

RawPacket RtcpPacket::Build() const {
  size_t length = 0;
  uint8_t packet[IP_PACKET_SIZE];
  CreateAndAddAppended(packet, &length, IP_PACKET_SIZE);
  return RawPacket(packet, length);
}

void RtcpPacket::Build(uint8_t* packet,
                       size_t* length,
                       size_t max_length) const {
  *length = 0;
  CreateAndAddAppended(packet, length, max_length);
}

void RtcpPacket::CreateAndAddAppended(uint8_t* packet,
                                      size_t* length,
                                      size_t max_length) const {
  Create(packet, length, max_length);
  for (std::vector<RtcpPacket*>::const_iterator it = appended_packets_.begin();
      it != appended_packets_.end(); ++it) {
    (*it)->CreateAndAddAppended(packet, length, max_length);
  }
}

void Empty::Create(uint8_t* packet, size_t* length, size_t max_length) const {
}

void SenderReport::Create(uint8_t* packet,
                          size_t* length,
                          size_t max_length) const {
  if (*length + BlockLength() > max_length) {
    LOG(LS_WARNING) << "Max packet size reached.";
    return;
  }
  CreateSenderReport(sr_, BlockToHeaderLength(BlockLength()), packet, length);
  CreateReportBlocks(report_blocks_, packet, length);
}

void SenderReport::WithReportBlock(ReportBlock* block) {
  assert(block);
  if (report_blocks_.size() >= kMaxNumberOfReportBlocks) {
    LOG(LS_WARNING) << "Max report blocks reached.";
    return;
  }
  report_blocks_.push_back(block->report_block_);
  sr_.NumberOfReportBlocks = report_blocks_.size();
}

void ReceiverReport::Create(uint8_t* packet,
                            size_t* length,
                            size_t max_length) const {
  if (*length + BlockLength() > max_length) {
    LOG(LS_WARNING) << "Max packet size reached.";
    return;
  }
  CreateReceiverReport(rr_, BlockToHeaderLength(BlockLength()), packet, length);
  CreateReportBlocks(report_blocks_, packet, length);
}

void ReceiverReport::WithReportBlock(ReportBlock* block) {
  assert(block);
  if (report_blocks_.size() >= kMaxNumberOfReportBlocks) {
    LOG(LS_WARNING) << "Max report blocks reached.";
    return;
  }
  report_blocks_.push_back(block->report_block_);
  rr_.NumberOfReportBlocks = report_blocks_.size();
}

void Ij::Create(uint8_t* packet, size_t* length, size_t max_length) const {
  if (*length + BlockLength() > max_length) {
    LOG(LS_WARNING) << "Max packet size reached.";
    return;
  }
  CreateIj(ij_items_, packet, length);
}

void Ij::WithJitterItem(uint32_t jitter) {
  if (ij_items_.size() >= kMaxNumberOfIjItems) {
    LOG(LS_WARNING) << "Max inter-arrival jitter items reached.";
    return;
  }
  ij_items_.push_back(jitter);
}

void Sdes::Create(uint8_t* packet, size_t* length, size_t max_length) const {
  assert(!chunks_.empty());
  if (*length + BlockLength() > max_length) {
    LOG(LS_WARNING) << "Max packet size reached.";
    return;
  }
  CreateSdes(chunks_, BlockToHeaderLength(BlockLength()), packet, length);
}

void Sdes::WithCName(uint32_t ssrc, std::string cname) {
  assert(cname.length() <= 0xff);
  if (chunks_.size() >= kMaxNumberOfChunks) {
    LOG(LS_WARNING) << "Max SDES chunks reached.";
    return;
  }
  // In each chunk, the list of items must be terminated by one or more null
  // octets. The next chunk must start on a 32-bit boundary.
  // CNAME (1 byte) | length (1 byte) | name | padding.
  int null_octets = 4 - ((2 + cname.length()) % 4);
  Chunk chunk;
  chunk.ssrc = ssrc;
  chunk.name = cname;
  chunk.null_octets = null_octets;
  chunks_.push_back(chunk);
}

size_t Sdes::BlockLength() const {
  // Header (4 bytes).
  // Chunk:
  // SSRC/CSRC (4 bytes) | CNAME (1 byte) | length (1 byte) | name | padding.
  size_t length = kHeaderLength;
  for (std::vector<Chunk>::const_iterator it = chunks_.begin();
       it != chunks_.end(); ++it) {
    length += 6 + (*it).name.length() + (*it).null_octets;
  }
  assert(length % 4 == 0);
  return length;
}

void Bye::Create(uint8_t* packet, size_t* length, size_t max_length) const {
  if (*length + BlockLength() > max_length) {
    LOG(LS_WARNING) << "Max packet size reached.";
    return;
  }
  CreateBye(bye_, csrcs_, BlockToHeaderLength(BlockLength()), packet, length);
}

void Bye::WithCsrc(uint32_t csrc) {
  if (csrcs_.size() >= kMaxNumberOfCsrcs) {
    LOG(LS_WARNING) << "Max CSRC size reached.";
    return;
  }
  csrcs_.push_back(csrc);
}

void App::Create(uint8_t* packet, size_t* length, size_t max_length) const {
  if (*length + BlockLength() > max_length) {
    LOG(LS_WARNING) << "Max packet size reached.";
    return;
  }
  CreateApp(app_, ssrc_, BlockToHeaderLength(BlockLength()), packet, length);
}

void Pli::Create(uint8_t* packet, size_t* length, size_t max_length) const {
  if (*length + BlockLength() > max_length) {
    LOG(LS_WARNING) << "Max packet size reached.";
    return;
  }
  CreatePli(pli_, BlockToHeaderLength(BlockLength()), packet, length);
}

void Sli::Create(uint8_t* packet, size_t* length, size_t max_length) const {
  if (*length + BlockLength() > max_length) {
    LOG(LS_WARNING) << "Max packet size reached.";
    return;
  }
  CreateSli(sli_, sli_item_, BlockToHeaderLength(BlockLength()), packet,
            length);
}

void Nack::Create(uint8_t* packet, size_t* length, size_t max_length) const {
  assert(!nack_fields_.empty());
  if (*length + BlockLength() > max_length) {
    LOG(LS_WARNING) << "Max packet size reached.";
    return;
  }
  CreateNack(nack_, nack_fields_, BlockToHeaderLength(BlockLength()), packet,
             length);
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

void Rpsi::Create(uint8_t* packet, size_t* length, size_t max_length) const {
  assert(rpsi_.NumberOfValidBits > 0);
  if (*length + BlockLength() > max_length) {
    LOG(LS_WARNING) << "Max packet size reached.";
    return;
  }
  CreateRpsi(rpsi_, padding_bytes_, BlockToHeaderLength(BlockLength()), packet,
             length);
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

void Fir::Create(uint8_t* packet, size_t* length, size_t max_length) const {
  if (*length + BlockLength() > max_length) {
    LOG(LS_WARNING) << "Max packet size reached.";
    return;
  }
  CreateFir(fir_, fir_item_, BlockToHeaderLength(BlockLength()), packet,
            length);
}
}  // namespace rtcp
}  // namespace webrtc
