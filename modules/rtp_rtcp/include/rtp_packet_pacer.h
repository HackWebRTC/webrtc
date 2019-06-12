/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_INCLUDE_RTP_PACKET_PACER_H_
#define MODULES_RTP_RTCP_INCLUDE_RTP_PACKET_PACER_H_

#include <memory>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
namespace webrtc {

// Interface for a paced sender, as implemented in the pacing module.
// This intended to replace the RtpPacketSender interface defined in
// rtp_rtcp_defines.h
// TODO(bugs.webrtc.org/10633): Add things missing to this interface so that we
// can use multiple different pacer implementations, and stop inheriting from
// RtpPacketSender.
class RtpPacketPacer : RtpPacketSender {
 public:
  RtpPacketPacer() = default;
  ~RtpPacketPacer() override;

  // Insert packet into queue, for eventual transmission. Based on the type of
  // the packet, it will prioritized and scheduled relative to other packets and
  // the current target send rate.
  virtual void EnqueuePacket(std::unique_ptr<RtpPacketToSend> packet) = 0;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_INCLUDE_RTP_PACKET_PACER_H_
