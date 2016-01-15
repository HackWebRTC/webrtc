/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_FIR_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_FIR_H_

#include "webrtc/base/basictypes.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"

namespace webrtc {
namespace rtcp {

// Full intra request (FIR) (RFC 5104).
class Fir : public RtcpPacket {
 public:
  Fir() : RtcpPacket() {
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
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  size_t BlockLength() const {
    const size_t kFciLength = 8;
    return kCommonFbFmtLength + kFciLength;
  }

  RTCPUtility::RTCPPacketPSFBFIR fir_;
  RTCPUtility::RTCPPacketPSFBFIRItem fir_item_;
};

}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_FIR_H_
