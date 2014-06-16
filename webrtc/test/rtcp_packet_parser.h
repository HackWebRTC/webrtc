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
#include <string>
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

class Ij : public PacketType {
 public:
  Ij() {}
  virtual ~Ij() {}

 private:
  friend class RtcpPacketParser;

  void Set() { ++num_packets_; }
};

class IjItem : public PacketType {
 public:
  IjItem() {}
  virtual ~IjItem() {}

  uint32_t Jitter() const { return ij_item_.Jitter; }

 private:
  friend class RtcpPacketParser;

  void Set(const RTCPUtility::RTCPPacketExtendedJitterReportItem& ij_item) {
    ij_item_ = ij_item;
    ++num_packets_;
  }

  RTCPUtility::RTCPPacketExtendedJitterReportItem ij_item_;
};

class Sdes : public PacketType {
 public:
  Sdes() {}
  virtual ~Sdes() {}

 private:
  friend class RtcpPacketParser;

  void Set() { ++num_packets_; }
};

class SdesChunk : public PacketType {
 public:
  SdesChunk() {}
  virtual ~SdesChunk() {}

  uint32_t Ssrc() const { return cname_.SenderSSRC; }
  std::string Cname() const { return cname_.CName; }

 private:
  friend class RtcpPacketParser;

  void Set(const RTCPUtility::RTCPPacketSDESCName& cname) {
    cname_ = cname;
    ++num_packets_;
  }

  RTCPUtility::RTCPPacketSDESCName cname_;
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

class App : public PacketType {
 public:
  App() {}
  virtual ~App() {}

  uint8_t SubType() const { return app_.SubType; }
  uint32_t Name() const { return app_.Name; }

 private:
  friend class RtcpPacketParser;

  void Set(const RTCPUtility::RTCPPacketAPP& app) {
    app_ = app;
    ++num_packets_;
  }

  RTCPUtility::RTCPPacketAPP app_;
};

class AppItem : public PacketType {
 public:
  AppItem() {}
  virtual ~AppItem() {}

  uint8_t* Data() { return app_item_.Data; }
  uint16_t DataLength() const { return app_item_.Size; }

 private:
  friend class RtcpPacketParser;

  void Set(const RTCPUtility::RTCPPacketAPP& app) {
    app_item_ = app;
    ++num_packets_;
  }

  RTCPUtility::RTCPPacketAPP app_item_;
};

class Pli : public PacketType {
 public:
  Pli() {}
  virtual ~Pli() {}

  uint32_t Ssrc() const { return pli_.SenderSSRC; }
  uint32_t MediaSsrc() const { return pli_.MediaSSRC; }

 private:
  friend class RtcpPacketParser;

  void Set(const RTCPUtility::RTCPPacketPSFBPLI& pli) {
    pli_ = pli;
    ++num_packets_;
  }

  RTCPUtility::RTCPPacketPSFBPLI pli_;
};

class Sli : public PacketType {
 public:
  Sli() {}
  virtual ~Sli() {}

  uint32_t Ssrc() const { return sli_.SenderSSRC; }
  uint32_t MediaSsrc() const { return sli_.MediaSSRC; }

 private:
  friend class RtcpPacketParser;

  void Set(const RTCPUtility::RTCPPacketPSFBSLI& sli) {
    sli_ = sli;
    ++num_packets_;
  }

  RTCPUtility::RTCPPacketPSFBSLI sli_;
};

class SliItem : public PacketType {
 public:
  SliItem() {}
  virtual ~SliItem() {}

  uint16_t FirstMb() const { return sli_item_.FirstMB; }
  uint16_t NumberOfMb() const { return sli_item_.NumberOfMB; }
  uint8_t PictureId() const { return sli_item_.PictureId; }

 private:
  friend class RtcpPacketParser;

  void Set(const RTCPUtility::RTCPPacketPSFBSLIItem& sli_item) {
    sli_item_ = sli_item;
    ++num_packets_;
  }

  RTCPUtility::RTCPPacketPSFBSLIItem sli_item_;
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
  Sdes* sdes() { return &sdes_; }
  SdesChunk* sdes_chunk() { return &sdes_chunk_; }
  Bye* bye() { return &bye_; }
  App* app() { return &app_; }
  AppItem* app_item() { return &app_item_; }
  Ij* ij() { return &ij_; }
  IjItem* ij_item() { return &ij_item_; }
  Pli* pli() { return &pli_; }
  Sli* sli() { return &sli_; }
  SliItem* sli_item() { return &sli_item_; }
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
  Sdes sdes_;
  SdesChunk sdes_chunk_;
  Bye bye_;
  App app_;
  AppItem app_item_;
  Ij ij_;
  IjItem ij_item_;
  Pli pli_;
  Sli sli_;
  SliItem sli_item_;
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
