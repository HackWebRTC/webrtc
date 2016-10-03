/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_FEC_TEST_HELPER_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_FEC_TEST_HELPER_H_

#include "webrtc/base/basictypes.h"
#include "webrtc/base/random.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/rtp_rtcp/source/forward_error_correction.h"

namespace webrtc {
namespace test {
namespace fec {

struct RawRtpPacket : public ForwardErrorCorrection::Packet {
  WebRtcRTPHeader header;
};

// This class generates media packets corresponding to a single frame.
class MediaPacketGenerator {
 public:
  MediaPacketGenerator(uint32_t min_packet_size,
                       uint32_t max_packet_size,
                       uint32_t ssrc,
                       Random* random)
      : min_packet_size_(min_packet_size),
        max_packet_size_(max_packet_size),
        ssrc_(ssrc),
        random_(random) {}

  // Construct the media packets, up to |num_media_packets| packets.
  ForwardErrorCorrection::PacketList ConstructMediaPackets(
      int num_media_packets,
      uint16_t start_seq_num);
  ForwardErrorCorrection::PacketList ConstructMediaPackets(
      int num_media_packets);

  uint16_t GetFecSeqNum();

 private:
  uint32_t min_packet_size_;
  uint32_t max_packet_size_;
  uint32_t ssrc_;
  Random* random_;

  ForwardErrorCorrection::PacketList media_packets_;
  uint16_t fec_seq_num_;
};

// This class generates media and ULPFEC packets (both encapsulated in RED)
// for a single frame.
class UlpfecPacketGenerator {
 public:
  UlpfecPacketGenerator();

  void NewFrame(int num_packets);

  uint16_t NextSeqNum();

  RawRtpPacket* NextPacket(int offset, size_t length);

  // Creates a new RtpPacket with the RED header added to the packet.
  RawRtpPacket* BuildMediaRedPacket(const RawRtpPacket* packet);

  // Creates a new RtpPacket with FEC payload and red header. Does this by
  // creating a new fake media RtpPacket, clears the marker bit and adds a RED
  // header. Finally replaces the payload with the content of |packet->data|.
  RawRtpPacket* BuildFecRedPacket(const ForwardErrorCorrection::Packet* packet);

  void SetRedHeader(ForwardErrorCorrection::Packet* red_packet,
                    uint8_t payload_type,
                    size_t header_length) const;

 private:
  static void BuildRtpHeader(uint8_t* data, const RTPHeader* header);

  int num_packets_;
  uint16_t seq_num_;
  uint32_t timestamp_;
};

}  // namespace fec
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_FEC_TEST_HELPER_H_
