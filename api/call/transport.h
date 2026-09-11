/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_CALL_TRANSPORT_H_
#define API_CALL_TRANSPORT_H_

#include <stdint.h>

#include <cstddef>
#include <span>

namespace webrtc {

// TODO(holmer): Look into unifying this with the PacketOptions in
// asyncpacketsocket.h.
struct PacketOptions {
  PacketOptions();
  PacketOptions(const PacketOptions&);
  ~PacketOptions();

  // Negative ids are invalid and should be interpreted
  // as packet_id not being set.
  int64_t packet_id = -1;
  // Whether this is an audio or video packet, excluding retransmissions.
  // Defaults to `false` which is the more common case.
  bool is_media = false;
  bool included_in_feedback = false;
  bool included_in_allocation = false;
  bool send_as_ect1 = false;
  // Whether this packet can be part of a packet batch at lower levels.
  bool batchable = false;
  // Whether this packet is the last of a batch.
  bool last_packet_in_batch = false;
};

// Describes one packetized RTP access unit before sequence numbers are
// allocated. Transports with frame-level admission may reject the whole batch
// here instead of creating a partially visible RTP sequence range.
struct RtpPacketBatch {
  uint32_t ssrc = 0;
  uint32_t timestamp = 0;
  size_t packet_count = 0;
  size_t byte_size = 0;
  bool key_frame = false;
};

enum class RtpPacingMode {
  kLibwebrtc,
  kExternalTransport,
};

class Transport {
 public:
  virtual RtpPacingMode GetRtpPacingMode() const {
    return RtpPacingMode::kLibwebrtc;
  }
  virtual bool ReserveRtpBatch(const RtpPacketBatch&) { return true; }
  virtual bool SendRtp(std::span<const uint8_t> packet,
                       const PacketOptions& options) = 0;
  virtual bool SendRtcp(std::span<const uint8_t> packet,
                        const PacketOptions& options) = 0;

 protected:
  virtual ~Transport() = default;
};

}  // namespace webrtc

#endif  // API_CALL_TRANSPORT_H_
