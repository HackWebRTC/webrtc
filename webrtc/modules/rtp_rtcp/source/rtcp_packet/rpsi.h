/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_RPSI_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_RPSI_H_

#include "webrtc/base/basictypes.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"

namespace webrtc {
namespace rtcp {
// Reference picture selection indication (RPSI) (RFC 4585).
class Rpsi : public RtcpPacket {
 public:
  Rpsi()
      : RtcpPacket(),
        padding_bytes_(0) {
    memset(&rpsi_, 0, sizeof(rpsi_));
  }

  virtual ~Rpsi() {}

  void From(uint32_t ssrc) {
    rpsi_.SenderSSRC = ssrc;
  }
  void To(uint32_t ssrc) {
    rpsi_.MediaSSRC = ssrc;
  }
  void WithPayloadType(uint8_t payload) {
    assert(payload <= 0x7f);
    rpsi_.PayloadType = payload;
  }
  void WithPictureId(uint64_t picture_id);

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  size_t BlockLength() const {
    size_t fci_length = 2 + (rpsi_.NumberOfValidBits / 8) + padding_bytes_;
    return kCommonFbFmtLength + fci_length;
  }

  uint8_t padding_bytes_;
  RTCPUtility::RTCPPacketPSFBRPSI rpsi_;

  RTC_DISALLOW_COPY_AND_ASSIGN(Rpsi);
};
}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_RPSI_H_
