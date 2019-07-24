/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_INCLUDE_RTP_PACKET_SENDER_H_
#define MODULES_RTP_RTCP_INCLUDE_RTP_PACKET_SENDER_H_

#include <memory>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"

namespace webrtc {

// TODO(bugs.webrtc.org/10633): Remove Priority and InsertPacket when old pacer
// code path is gone.
class RtpPacketSender {
 public:
  virtual ~RtpPacketSender() = default;

  // These are part of the legacy PacedSender interface and will be removed.
  enum Priority {
    kHighPriority = 0,    // Pass through; will be sent immediately.
    kNormalPriority = 2,  // Put in back of the line.
    kLowPriority = 3,     // Put in back of the low priority line.
  };

  // Adds the packet information to the queue and call TimeToSendPacket when
  // it's time to send.
  virtual void InsertPacket(Priority priority,
                            uint32_t ssrc,
                            uint16_t sequence_number,
                            int64_t capture_time_ms,
                            size_t bytes,
                            bool retransmission) = 0;

  // Insert packet into queue, for eventual transmission. Based on the type of
  // the packet, it will be prioritized and scheduled relative to other packets
  // and the current target send rate.
  virtual void EnqueuePacket(std::unique_ptr<RtpPacketToSend> packet) = 0;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_INCLUDE_RTP_PACKET_SENDER_H_
