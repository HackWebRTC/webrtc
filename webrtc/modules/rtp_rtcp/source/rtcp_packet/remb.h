/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_REMB_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_REMB_H_

#include <vector>
#include "webrtc/base/basictypes.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"

namespace webrtc {
namespace rtcp {
// Receiver Estimated Max Bitrate (REMB) (draft-alvestrand-rmcat-remb).
class Remb : public RtcpPacket {
 public:
  Remb() : RtcpPacket() {
    memset(&remb_, 0, sizeof(remb_));
    memset(&remb_item_, 0, sizeof(remb_item_));
  }

  virtual ~Remb() {}

  void From(uint32_t ssrc) {
    remb_.SenderSSRC = ssrc;
  }
  void AppliesTo(uint32_t ssrc);

  void WithBitrateBps(uint32_t bitrate_bps) {
    remb_item_.BitRate = bitrate_bps;
  }

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  static const int kMaxNumberOfSsrcs = 0xff;

  size_t BlockLength() const {
    return (remb_item_.NumberOfSSRCs + 5) * 4;
  }

  RTCPUtility::RTCPPacketPSFBAPP remb_;
  RTCPUtility::RTCPPacketPSFBREMBItem remb_item_;

  RTC_DISALLOW_COPY_AND_ASSIGN(Remb);
};
}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_REMB_H_
