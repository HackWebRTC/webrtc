/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_INCLUDE_FLEXFEC_RECEIVER_H_
#define WEBRTC_MODULES_RTP_RTCP_INCLUDE_FLEXFEC_RECEIVER_H_

#include <memory>

#include "webrtc/base/basictypes.h"
#include "webrtc/modules/rtp_rtcp/include/ulpfec_receiver.h"

namespace webrtc {

// Callback interface for packets recovered by FlexFEC. The implementation
// should be able to demultiplex the recovered RTP packets based on SSRC.
class RecoveredPacketReceiver {
 public:
  virtual bool OnRecoveredPacket(const uint8_t* packet, size_t length) = 0;

 protected:
  virtual ~RecoveredPacketReceiver() = default;
};

class FlexfecReceiver {
 public:
  static std::unique_ptr<FlexfecReceiver> Create(
      uint32_t flexfec_ssrc,
      uint32_t protected_media_ssrc,
      RecoveredPacketReceiver* callback);
  virtual ~FlexfecReceiver();

  // Inserts a received packet (can be either media or FlexFEC) into the
  // internal buffer, and sends the received packets to the erasure code.
  // All newly recovered packets are sent back through the callback.
  virtual bool AddAndProcessReceivedPacket(const uint8_t* packet,
                                           size_t packet_length) = 0;

  // Returns a counter describing the added and recovered packets.
  virtual FecPacketCounter GetPacketCounter() const = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_INCLUDE_FLEXFEC_RECEIVER_H_
