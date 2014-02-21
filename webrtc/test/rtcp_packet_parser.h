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

class SenderReport {
 public:
  SenderReport() : num_packets_(0) {}
  ~SenderReport() {}

  int num_packets() { return num_packets_; }
  uint32_t Ssrc() { return sr_.SenderSSRC; }
  uint32_t NtpSec() { return sr_.NTPMostSignificant; }
  uint32_t NtpFrac() { return sr_.NTPLeastSignificant; }
  uint32_t RtpTimestamp() { return sr_.RTPTimestamp; }
  uint32_t PacketCount() { return sr_.SenderPacketCount; }
  uint32_t OctetCount() { return sr_.SenderOctetCount; }

 private:
  friend class RtcpPacketParser;
  void Set(const RTCPUtility::RTCPPacketSR& sr) {
    sr_ = sr;
    ++num_packets_;
  }

  int num_packets_;
  RTCPUtility::RTCPPacketSR sr_;
};

class ReceiverReport {
 public:
  ReceiverReport() : num_packets_(0) {}
  ~ReceiverReport() {}

  int num_packets() { return num_packets_; }
  uint32_t Ssrc() { return rr_.SenderSSRC; }

 private:
  friend class RtcpPacketParser;
  void Set(const RTCPUtility::RTCPPacketRR& rr) {
    rr_ = rr;
    ++num_packets_;
  }

  int num_packets_;
  RTCPUtility::RTCPPacketRR rr_;
};

class ReportBlock {
 public:
  ReportBlock() : num_packets_(0) {}
  ~ReportBlock() {}

  int num_packets() { return num_packets_; }
  uint32_t Ssrc() { return rb_.SSRC; }
  uint8_t FractionLost() { return rb_.FractionLost; }
  uint32_t CumPacketLost() { return rb_.CumulativeNumOfPacketsLost; }
  uint32_t ExtHighestSeqNum() { return rb_.ExtendedHighestSequenceNumber; }
  uint32_t Jitter() { return rb_.Jitter; }
  uint32_t LastSr() { return rb_.LastSR; }
  uint32_t DelayLastSr() { return rb_.DelayLastSR; }

 private:
  friend class RtcpPacketParser;
  void Set(const RTCPUtility::RTCPPacketReportBlockItem& rb) {
    rb_ = rb;
    ++num_packets_;
  }

  int num_packets_;
  RTCPUtility::RTCPPacketReportBlockItem rb_;
};

class Bye {
 public:
  Bye() : num_packets_(0) {}
  ~Bye() {}

  int num_packets() { return num_packets_; }
  uint32_t Ssrc() { return bye_.SenderSSRC; }

 private:
  friend class RtcpPacketParser;
  void Set(const RTCPUtility::RTCPPacketBYE& bye) {
    bye_ = bye;
    ++num_packets_;
  }

  int num_packets_;
  RTCPUtility::RTCPPacketBYE bye_;
};

class Fir {
 public:
  Fir() : num_packets_(0) {}
  ~Fir() {}

  int num_packets() { return num_packets_; }
  uint32_t Ssrc() { return fir_.SenderSSRC; }

 private:
  friend class RtcpPacketParser;
  void Set(const RTCPUtility::RTCPPacketPSFBFIR& fir) {
    fir_ = fir;
    ++num_packets_;
  }

  int num_packets_;
  RTCPUtility::RTCPPacketPSFBFIR fir_;
};

class FirItem {
 public:
  FirItem() : num_packets_(0) {}
  ~FirItem() {}

  int num_packets() { return num_packets_; }
  uint32_t Ssrc() { return fir_item_.SSRC; }
  uint8_t SeqNum() { return fir_item_.CommandSequenceNumber; }

 private:
  friend class RtcpPacketParser;
  void Set(const RTCPUtility::RTCPPacketPSFBFIRItem& fir_item) {
    fir_item_ = fir_item;
    ++num_packets_;
  }

  int num_packets_;
  RTCPUtility::RTCPPacketPSFBFIRItem fir_item_;
};

class Nack {
 public:
  Nack() : num_packets_(0) {}
  ~Nack() {}

  int num_packets() { return num_packets_; }
  uint32_t Ssrc() { return nack_.SenderSSRC; }
  uint32_t MediaSsrc() { return nack_.MediaSSRC; }

 private:
  friend class RtcpPacketParser;
  void Set(const RTCPUtility::RTCPPacketRTPFBNACK& nack) {
    nack_ = nack;
    ++num_packets_;
  }

  int num_packets_;
  RTCPUtility::RTCPPacketRTPFBNACK nack_;
};

class NackItem {
 public:
  NackItem() : num_packets_(0) {}
  ~NackItem() {}

  int num_packets() { return num_packets_; }
  std::vector<uint16_t> last_nack_list() {
    assert(num_packets_ > 0);
    return last_nack_list_;
  }

 private:
  friend class RtcpPacketParser;
  void Set(const RTCPUtility::RTCPPacketRTPFBNACKItem& nack_item) {
    last_nack_list_.clear();
    last_nack_list_.push_back(nack_item.PacketID);
    for (int i = 0; i < 16; ++i) {
      if (nack_item.BitMask & (1 << i)) {
        last_nack_list_.push_back(nack_item.PacketID + i + 1);
      }
    }
    ++num_packets_;
  }

  int num_packets_;
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
  Fir fir_;
  FirItem fir_item_;
  Nack nack_;
  NackItem nack_item_;

  std::map<uint32_t, int> report_blocks_per_ssrc_;
};
}  // namespace test
}  // namespace webrtc
#endif  // WEBRTC_TEST_RTCP_PACKET_PARSER_H_
