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

#ifndef WEBRTC_TEST_RTCP_PACKET_PARSER_H_
#define WEBRTC_TEST_RTCP_PACKET_PARSER_H_

#include <map>
#include <vector>

#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace test {

class RtcpPacketParser;

class PacketType {
 public:
  virtual ~PacketType() {}

  int num_packets() const { return num_packets_; }

 protected:
  PacketType() : num_packets_(0) {}

  int num_packets_;
};

class SenderReport : public PacketType {
 public:
  SenderReport() {}
  virtual ~SenderReport() {}

  uint32_t Ssrc() const { return sr_.SenderSSRC; }
  uint32_t NtpSec() const { return sr_.NTPMostSignificant; }
  uint32_t NtpFrac() const { return sr_.NTPLeastSignificant; }
  uint32_t RtpTimestamp() const { return sr_.RTPTimestamp; }
  uint32_t PacketCount() const { return sr_.SenderPacketCount; }
  uint32_t OctetCount() const { return sr_.SenderOctetCount; }

 private:
  friend class RtcpPacketParser;

  void Set(const RTCPUtility::RTCPPacketSR& sr) {
    sr_ = sr;
    ++num_packets_;
  }

  RTCPUtility::RTCPPacketSR sr_;
};

class ReceiverReport : public PacketType {
 public:
  ReceiverReport() {}
  virtual ~ReceiverReport() {}

  uint32_t Ssrc() const { return rr_.SenderSSRC; }

 private:
  friend class RtcpPacketParser;

  void Set(const RTCPUtility::RTCPPacketRR& rr) {
    rr_ = rr;
    ++num_packets_;
  }

  RTCPUtility::RTCPPacketRR rr_;
};

class ReportBlock : public PacketType {
 public:
  ReportBlock() {}
  virtual ~ReportBlock() {}

  uint32_t Ssrc() const { return rb_.SSRC; }
  uint8_t FractionLost() const { return rb_.FractionLost; }
  uint32_t CumPacketLost() const { return rb_.CumulativeNumOfPacketsLost; }
  uint32_t ExtHighestSeqNum() const { return rb_.ExtendedHighestSequenceNumber;}
  uint32_t Jitter() const { return rb_.Jitter; }
  uint32_t LastSr() const { return rb_.LastSR; }
  uint32_t DelayLastSr()const  { return rb_.DelayLastSR; }

 private:
  friend class RtcpPacketParser;

  void Set(const RTCPUtility::RTCPPacketReportBlockItem& rb) {
    rb_ = rb;
    ++num_packets_;
  }

  RTCPUtility::RTCPPacketReportBlockItem rb_;
};

class Bye : public PacketType {
 public:
  Bye() {}
  virtual ~Bye() {}

  uint32_t Ssrc() const { return bye_.SenderSSRC; }

 private:
  friend class RtcpPacketParser;

  void Set(const RTCPUtility::RTCPPacketBYE& bye) {
    bye_ = bye;
    ++num_packets_;
  }

  RTCPUtility::RTCPPacketBYE bye_;
};

class Rpsi : public PacketType {
 public:
  Rpsi() {}
  virtual ~Rpsi() {}

  uint32_t Ssrc() const { return rpsi_.SenderSSRC; }
  uint32_t MediaSsrc() const { return rpsi_.MediaSSRC; }
  uint8_t PayloadType() const { return rpsi_.PayloadType; }
  uint16_t NumberOfValidBits() const { return rpsi_.NumberOfValidBits; }
  uint64_t PictureId() const;

 private:
  friend class RtcpPacketParser;

  void Set(const RTCPUtility::RTCPPacketPSFBRPSI& rpsi) {
    rpsi_ = rpsi;
    ++num_packets_;
  }

  RTCPUtility::RTCPPacketPSFBRPSI rpsi_;
};

class Fir : public PacketType {
 public:
  Fir() {}
  virtual ~Fir() {}

  uint32_t Ssrc() const { return fir_.SenderSSRC; }

 private:
  friend class RtcpPacketParser;

  void Set(const RTCPUtility::RTCPPacketPSFBFIR& fir) {
    fir_ = fir;
    ++num_packets_;
  }

  RTCPUtility::RTCPPacketPSFBFIR fir_;
};

class FirItem : public PacketType {
 public:
  FirItem() {}
  virtual ~FirItem() {}

  uint32_t Ssrc() const { return fir_item_.SSRC; }
  uint8_t SeqNum() const { return fir_item_.CommandSequenceNumber; }

 private:
  friend class RtcpPacketParser;

  void Set(const RTCPUtility::RTCPPacketPSFBFIRItem& fir_item) {
    fir_item_ = fir_item;
    ++num_packets_;
  }

  RTCPUtility::RTCPPacketPSFBFIRItem fir_item_;
};

class Nack : public PacketType {
 public:
  Nack() {}
  virtual ~Nack() {}

  uint32_t Ssrc() const { return nack_.SenderSSRC; }
  uint32_t MediaSsrc() const { return nack_.MediaSSRC; }

 private:
  friend class RtcpPacketParser;

  void Set(const RTCPUtility::RTCPPacketRTPFBNACK& nack) {
    nack_ = nack;
    ++num_packets_;
  }

  RTCPUtility::RTCPPacketRTPFBNACK nack_;
};

class NackItem : public PacketType {
 public:
  NackItem() {}
  virtual ~NackItem() {}

  std::vector<uint16_t> last_nack_list() const {
    return last_nack_list_;
  }

 private:
  friend class RtcpPacketParser;

  void Set(const RTCPUtility::RTCPPacketRTPFBNACKItem& nack_item) {
    last_nack_list_.push_back(nack_item.PacketID);
    for (int i = 0; i < 16; ++i) {
      if (nack_item.BitMask & (1 << i)) {
        last_nack_list_.push_back(nack_item.PacketID + i + 1);
      }
    }
    ++num_packets_;
  }
  void Clear() { last_nack_list_.clear(); }

  std::vector<uint16_t> last_nack_list_;
};

class RtcpPacketParser {
 public:
  RtcpPacketParser();
  ~RtcpPacketParser();

  void Parse(const void *packet, int packet_len);

  SenderReport* sender_report() { return &sender_report_; }
  ReceiverReport* receiver_report() { return &receiver_report_; }
  ReportBlock* report_block() { return &report_block_; }
  Bye* bye() { return &bye_; }
  Rpsi* rpsi() { return &rpsi_; }
  Fir* fir() { return &fir_; }
  FirItem* fir_item() { return &fir_item_; }
  Nack* nack() { return &nack_; }
  NackItem* nack_item() { return &nack_item_; }

  int report_blocks_per_ssrc(uint32_t ssrc) {
    return report_blocks_per_ssrc_[ssrc];
  }

 private:
  SenderReport sender_report_;
  ReceiverReport receiver_report_;
  ReportBlock report_block_;
  Bye bye_;
  Rpsi rpsi_;
  Fir fir_;
  FirItem fir_item_;
  Nack nack_;
  NackItem nack_item_;

  std::map<uint32_t, int> report_blocks_per_ssrc_;
};
}  // namespace test
}  // namespace webrtc
#endif  // WEBRTC_TEST_RTCP_PACKET_PARSER_H_
