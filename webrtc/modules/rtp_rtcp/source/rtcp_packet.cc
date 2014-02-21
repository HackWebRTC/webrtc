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
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {
namespace rtcp {
namespace {
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

// From RFC 3550, RTP: A Transport Protocol for Real-Time Applications.
//
//  Sender report
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

void CreateSenderReport(const RTCPUtility::RTCPPacketSR& sr,
                        uint8_t* buffer,
                        uint16_t* pos) {
  const uint16_t kLength = 6 + (6 * sr.NumberOfReportBlocks);
  AssignUWord8(buffer, pos, 0x80 + sr.NumberOfReportBlocks);
  AssignUWord8(buffer, pos, RTCPUtility::PT_SR);
  AssignUWord16(buffer, pos, kLength);
  AssignUWord32(buffer, pos, sr.SenderSSRC);
  AssignUWord32(buffer, pos, sr.NTPMostSignificant);
  AssignUWord32(buffer, pos, sr.NTPLeastSignificant);
  AssignUWord32(buffer, pos, sr.RTPTimestamp);
  AssignUWord32(buffer, pos, sr.SenderPacketCount);
  AssignUWord32(buffer, pos, sr.SenderOctetCount);
}

//  Receiver report, header (RFC 3550).
//
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P|    RC   |   PT=RR=201   |             length            |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                     SSRC of packet sender                     |
//  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+

void CreateReceiverReport(const RTCPUtility::RTCPPacketRR& rr,
                          uint8_t* buffer,
                          uint16_t* pos) {
  const uint16_t kLength =  1 + (6 * rr.NumberOfReportBlocks);
  AssignUWord8(buffer, pos, 0x80 + rr.NumberOfReportBlocks);
  AssignUWord8(buffer, pos, RTCPUtility::PT_RR);
  AssignUWord16(buffer, pos, kLength);
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

void CreateReportBlock(
    const RTCPUtility::RTCPPacketReportBlockItem& report_block,
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

void CreateBye(const RTCPUtility::RTCPPacketBYE& bye,
               const std::vector<uint32_t>& csrcs,
               uint8_t* buffer,
               uint16_t* pos) {
  const uint8_t kNumSsrcAndCsrcs = 1 + csrcs.size();
  AssignUWord8(buffer, pos, 0x80 + kNumSsrcAndCsrcs);
  AssignUWord8(buffer, pos, RTCPUtility::PT_BYE);
  AssignUWord16(buffer, pos, kNumSsrcAndCsrcs);
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

void CreateFirRequest(const RTCPUtility::RTCPPacketPSFBFIR& fir,
                      const RTCPUtility::RTCPPacketPSFBFIRItem& fir_item,
                      uint8_t* buffer,
                      uint16_t* pos) {
  const uint16_t kLength = 4;
  const uint8_t kFmt = 4;
  AssignUWord8(buffer, pos, 0x80 + kFmt);
  AssignUWord8(buffer, pos, RTCPUtility::PT_PSFB);
  AssignUWord16(buffer, pos, kLength);
  AssignUWord32(buffer, pos, fir.SenderSSRC);
  AssignUWord32(buffer, pos, 0);
  AssignUWord32(buffer, pos, fir_item.SSRC);
  AssignUWord8(buffer, pos, fir_item.CommandSequenceNumber);
  AssignUWord24(buffer, pos, 0);
}

void AppendReportBlocks(const std::vector<ReportBlock*>& report_blocks,
                        uint8_t* buffer,
                        uint16_t* pos) {
  for (std::vector<ReportBlock*>::const_iterator it = report_blocks.begin();
       it != report_blocks.end(); ++it) {
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
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, -1,
                 "Max packet size reached, skipped SR.");
    return;
  }
  CreateSenderReport(sr_, packet, len);
  AppendReportBlocks(report_blocks_, packet, len);
}

void SenderReport::WithReportBlock(ReportBlock* block) {
  assert(block);
  if (report_blocks_.size() >= kMaxNumberOfReportBlocks) {
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, -1,
                 "Max report block size reached.");
    return;
  }
  report_blocks_.push_back(block);
  sr_.NumberOfReportBlocks = report_blocks_.size();
}

void ReceiverReport::Create(uint8_t* packet,
                            uint16_t* len,
                            uint16_t max_len) const {
  if (*len + Length() > max_len) {
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, -1,
                 "Max packet size reached, skipped RR.");
    return;
  }
  CreateReceiverReport(rr_, packet, len);
  AppendReportBlocks(report_blocks_, packet, len);
}

void ReceiverReport::WithReportBlock(ReportBlock* block) {
  assert(block);
  if (report_blocks_.size() >= kMaxNumberOfReportBlocks) {
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, -1,
                 "Max report block size reached.");
    return;
  }
  report_blocks_.push_back(block);
  rr_.NumberOfReportBlocks = report_blocks_.size();
}

void Bye::Create(uint8_t* packet, uint16_t* len, uint16_t max_len) const {
  if (*len + Length() > max_len) {
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, -1,
                 "Max packet size reached, skipped BYE.");
    return;
  }
  CreateBye(bye_, csrcs_, packet, len);
}

void Bye::WithCsrc(uint32_t csrc) {
  if (csrcs_.size() >= kMaxNumberOfCsrcs) {
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, -1,
                 "Max CSRC size reached.");
    return;
  }
  csrcs_.push_back(csrc);
}

void Fir::Create(uint8_t* packet, uint16_t* len, uint16_t max_len) const {
  if (*len + Length() > max_len) {
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, -1,
                 "Max packet size reached, skipped FIR.");
    return;
  }
  CreateFirRequest(fir_, fir_item_, packet, len);
}

void ReportBlock::Create(uint8_t* packet, uint16_t* len) const {
  CreateReportBlock(report_block_, packet, len);
}
}  // namespace rtcp
}  // namespace webrtc
