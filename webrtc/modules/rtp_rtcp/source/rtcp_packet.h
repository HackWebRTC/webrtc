/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_RTCP_PACKET_H_
#define WEBRTC_MODULES_RTP_RTCP_RTCP_PACKET_H_

#include <vector>

#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace rtcp {

class RawPacket;
class ReportBlock;

// Class for building RTCP packets.
//
//  Example:
//  ReportBlock report_block;
//  report_block.To(234)
//  report_block.FractionLost(10);
//
//  ReceiverReport rr;
//  rr.From(123);
//  rr.WithReportBlock(&report_block)
//
//  Fir fir;
//  fir.From(123);
//  fir.To(234)
//  fir.WithCommandSeqNum(123);
//
//  uint16_t len = 0;                      // Builds an intra frame request
//  uint8_t packet[kPacketSize];           // with sequence number 123.
//  fir.Build(packet, &len, kPacketSize);
//
//  RawPacket packet = fir.Build();        // Returns a RawPacket holding
//                                         // the built rtcp packet.
//
//  rr.Append(&fir)                        // Builds a compound RTCP packet with
//  RawPacket packet = rr.Build();         // a receiver report, report block
//                                         // and fir message.

class RtcpPacket {
 public:
  virtual ~RtcpPacket() {}

  void Append(RtcpPacket* packet);

  RawPacket Build() const;

  void Build(uint8_t* packet, uint16_t* len, uint16_t max_len) const;

 protected:
  RtcpPacket() {}

  virtual void Create(
      uint8_t* packet, uint16_t* len, uint16_t max_len) const = 0;

  void CreateAndAddAppended(
      uint8_t* packet, uint16_t* len, uint16_t max_len) const;

 private:
  std::vector<RtcpPacket*> appended_packets_;
};

class Empty : public RtcpPacket {
 public:
  Empty() {}

  virtual ~Empty() {}

 protected:
  virtual void Create(uint8_t* packet, uint16_t* len, uint16_t max_len) const;
};

//// From RFC 3550, RTP: A Transport Protocol for Real-Time Applications.
//
// RTCP sender report (RFC 3550).
//
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
//  |                         report block(s)                       |
//  |                            ....                               |

class SenderReport : public RtcpPacket {
 public:
  SenderReport()
    : RtcpPacket() {
    memset(&sr_, 0, sizeof(sr_));
  }

  virtual ~SenderReport() {}

  void From(uint32_t ssrc) {
    sr_.SenderSSRC = ssrc;
  }
  void WithNtpSec(uint32_t sec) {
    sr_.NTPMostSignificant = sec;
  }
  void WithNtpFrac(uint32_t frac) {
    sr_.NTPLeastSignificant = frac;
  }
  void WithRtpTimestamp(uint32_t rtp_timestamp) {
    sr_.RTPTimestamp = rtp_timestamp;
  }
  void WithPacketCount(uint32_t packet_count) {
    sr_.SenderPacketCount = packet_count;
  }
  void WithOctetCount(uint32_t octet_count) {
    sr_.SenderOctetCount = octet_count;
  }
  void WithReportBlock(ReportBlock* block);

  enum { kMaxNumberOfReportBlocks = 0x1f };

 protected:
  virtual void Create(uint8_t* packet, uint16_t* len, uint16_t max_len) const;

 private:
  uint16_t Length() const {
    const uint16_t kSrBlockLen = 28;
    const uint16_t kReportBlockLen = 24;
    return kSrBlockLen + report_blocks_.size() * kReportBlockLen;
  }

  RTCPUtility::RTCPPacketSR sr_;
  std::vector<ReportBlock*> report_blocks_;
};

//
// RTCP receiver report (RFC 3550).
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P|    RC   |   PT=RR=201   |             length            |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                     SSRC of packet sender                     |
//  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//  |                         report block(s)                       |
//  |                            ....                               |

class ReceiverReport : public RtcpPacket {
 public:
  ReceiverReport()
    : RtcpPacket() {
    memset(&rr_, 0, sizeof(rr_));
  }

  virtual ~ReceiverReport() {}

  void From(uint32_t ssrc) {
    rr_.SenderSSRC = ssrc;
  }
  void WithReportBlock(ReportBlock* block);

  enum { kMaxNumberOfReportBlocks = 0x1f };

 protected:
  virtual void Create(uint8_t* packet, uint16_t* len, uint16_t max_len) const;

 private:
  uint16_t Length() const {
    const uint16_t kRrBlockLen = 8;
    const uint16_t kReportBlockLen = 24;
    return kRrBlockLen + report_blocks_.size() * kReportBlockLen;
  }

  RTCPUtility::RTCPPacketRR rr_;
  std::vector<ReportBlock*> report_blocks_;
};

//
// RTCP report block (RFC 3550).
//
//   0                   1                   2                   3
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

class ReportBlock {
 public:
  ReportBlock() {
    memset(&report_block_, 0, sizeof(report_block_));
  }

  ~ReportBlock() {}

  void To(uint32_t ssrc) {
    report_block_.SSRC = ssrc;
  }
  void WithFractionLost(uint8_t fraction_lost) {
    report_block_.FractionLost = fraction_lost;
  }
  void WithCumPacketsLost(uint32_t cum_packets_lost) {
    report_block_.CumulativeNumOfPacketsLost = cum_packets_lost;
  }
  void WithExtHighestSeqNum(uint32_t ext_highest_seq_num) {
    report_block_.ExtendedHighestSequenceNumber = ext_highest_seq_num;
  }
  void WithJitter(uint32_t jitter) {
    report_block_.Jitter = jitter;
  }
  void WithLastSr(uint32_t last_sr) {
    report_block_.LastSR = last_sr;
  }
  void WithDelayLastSr(uint32_t delay_last_sr) {
    report_block_.DelayLastSR = delay_last_sr;
  }

  void Create(uint8_t* array, uint16_t* cur_pos) const;

 private:
  RTCPUtility::RTCPPacketReportBlockItem report_block_;
};

//
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

class Bye : public RtcpPacket {
 public:
  Bye()
    : RtcpPacket() {
    memset(&bye_, 0, sizeof(bye_));
  }

  virtual ~Bye() {}

  void From(uint32_t ssrc) {
    bye_.SenderSSRC = ssrc;
  }
  void WithCsrc(uint32_t csrc);

  enum { kMaxNumberOfCsrcs = 0x1f - 1 };

 protected:
  virtual void Create(uint8_t* packet, uint16_t* len, uint16_t max_len) const;

 private:
  uint16_t Length() const {
    const uint16_t kByeBlockLen = 8 + 4*csrcs_.size();
    return kByeBlockLen;
  }

  RTCPUtility::RTCPPacketBYE bye_;
  std::vector<uint32_t> csrcs_;
};

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

class Fir : public RtcpPacket {
 public:
  Fir()
    : RtcpPacket() {
    memset(&fir_, 0, sizeof(fir_));
    memset(&fir_item_, 0, sizeof(fir_item_));
  }

  virtual ~Fir() {}

  void From(uint32_t ssrc) {
    fir_.SenderSSRC = ssrc;
  }
  void To(uint32_t ssrc) {
    fir_item_.SSRC = ssrc;
  }
  void WithCommandSeqNum(uint8_t seq_num) {
    fir_item_.CommandSequenceNumber = seq_num;
  }

 protected:
  virtual void Create(uint8_t* packet, uint16_t* len, uint16_t max_len) const;

 private:
  uint16_t Length() const {
    const uint16_t kFirBlockLen = 20;
    return kFirBlockLen;
  }

  RTCPUtility::RTCPPacketPSFBFIR fir_;
  RTCPUtility::RTCPPacketPSFBFIRItem fir_item_;
};

// Class holding a RTCP packet.
//
// Takes a built rtcp packet.
//  RawPacket raw_packet(buffer, len);
//
// To access the raw packet:
//  raw_packet.buffer();         - pointer to the raw packet
//  raw_packet.buffer_length();  - the length of the raw packet

class RawPacket {
 public:
  RawPacket(const uint8_t* buffer, uint16_t len) {
    assert(len <= IP_PACKET_SIZE);
    memcpy(packet_, buffer, len);
    packet_length_ = len;
  }

  const uint8_t* buffer() {
    return packet_;
  }
  uint16_t buffer_length() const {
    return packet_length_;
  }

 private:
  uint16_t packet_length_;
  uint8_t packet_[IP_PACKET_SIZE];
};

}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_RTCP_PACKET_H_
