/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/fec_test_helper.h"

#include <memory>
#include <utility>

#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"

namespace webrtc {
namespace test {
namespace fec {

namespace {
constexpr uint8_t kFecPayloadType = 96;
constexpr uint8_t kRedPayloadType = 97;
constexpr uint8_t kVp8PayloadType = 120;
}  // namespace

ForwardErrorCorrection::PacketList MediaPacketGenerator::ConstructMediaPackets(
    int num_media_packets,
    uint16_t start_seq_num) {
  RTC_DCHECK_GT(num_media_packets, 0);
  uint16_t seq_num = start_seq_num;
  int time_stamp = random_->Rand<int>();

  ForwardErrorCorrection::PacketList media_packets;

  for (int i = 0; i < num_media_packets; ++i) {
    std::unique_ptr<ForwardErrorCorrection::Packet> media_packet(
        new ForwardErrorCorrection::Packet());
    media_packet->length = random_->Rand(min_packet_size_, max_packet_size_);

    // Generate random values for the first 2 bytes
    media_packet->data[0] = random_->Rand<uint8_t>();
    media_packet->data[1] = random_->Rand<uint8_t>();

    // The first two bits are assumed to be 10 by the FEC encoder.
    // In fact the FEC decoder will set the two first bits to 10 regardless of
    // what they actually were. Set the first two bits to 10 so that a memcmp
    // can be performed for the whole restored packet.
    media_packet->data[0] |= 0x80;
    media_packet->data[0] &= 0xbf;

    // FEC is applied to a whole frame.
    // A frame is signaled by multiple packets without the marker bit set
    // followed by the last packet of the frame for which the marker bit is set.
    // Only push one (fake) frame to the FEC.
    media_packet->data[1] &= 0x7f;

    webrtc::ByteWriter<uint16_t>::WriteBigEndian(&media_packet->data[2],
                                                 seq_num);
    webrtc::ByteWriter<uint32_t>::WriteBigEndian(&media_packet->data[4],
                                                 time_stamp);
    webrtc::ByteWriter<uint32_t>::WriteBigEndian(&media_packet->data[8], ssrc_);

    // Generate random values for payload.
    for (size_t j = 12; j < media_packet->length; ++j) {
      media_packet->data[j] = random_->Rand<uint8_t>();
    }
    seq_num++;
    media_packets.push_back(std::move(media_packet));
  }
  // Last packet, set marker bit.
  ForwardErrorCorrection::Packet* media_packet = media_packets.back().get();
  RTC_DCHECK(media_packet);
  media_packet->data[1] |= 0x80;

  fec_seq_num_ = seq_num;

  return media_packets;
}

ForwardErrorCorrection::PacketList MediaPacketGenerator::ConstructMediaPackets(
    int num_media_packets) {
  return ConstructMediaPackets(num_media_packets, random_->Rand<uint16_t>());
}

uint16_t MediaPacketGenerator::GetFecSeqNum() {
  return fec_seq_num_;
}

UlpfecPacketGenerator::UlpfecPacketGenerator()
    : num_packets_(0), seq_num_(0), timestamp_(0) {}

void UlpfecPacketGenerator::NewFrame(int num_packets) {
  num_packets_ = num_packets;
  timestamp_ += 3000;
}

uint16_t UlpfecPacketGenerator::NextSeqNum() {
  return ++seq_num_;
}

RawRtpPacket* UlpfecPacketGenerator::NextPacket(int offset, size_t length) {
  RawRtpPacket* rtp_packet = new RawRtpPacket;
  for (size_t i = 0; i < length; ++i)
    rtp_packet->data[i + kRtpHeaderSize] = offset + i;
  rtp_packet->length = length + kRtpHeaderSize;
  memset(&rtp_packet->header, 0, sizeof(WebRtcRTPHeader));
  rtp_packet->header.frameType = kVideoFrameDelta;
  rtp_packet->header.header.headerLength = kRtpHeaderSize;
  rtp_packet->header.header.markerBit = (num_packets_ == 1);
  rtp_packet->header.header.sequenceNumber = seq_num_;
  rtp_packet->header.header.timestamp = timestamp_;
  rtp_packet->header.header.payloadType = kVp8PayloadType;
  BuildRtpHeader(rtp_packet->data, &rtp_packet->header.header);
  ++seq_num_;
  --num_packets_;
  return rtp_packet;
}

// Creates a new RtpPacket with the RED header added to the packet.
RawRtpPacket* UlpfecPacketGenerator::BuildMediaRedPacket(
    const RawRtpPacket* packet) {
  const size_t kHeaderLength = packet->header.header.headerLength;
  RawRtpPacket* red_packet = new RawRtpPacket;
  red_packet->header = packet->header;
  red_packet->length = packet->length + 1;  // 1 byte RED header.
  memset(red_packet->data, 0, red_packet->length);
  // Copy RTP header.
  memcpy(red_packet->data, packet->data, kHeaderLength);
  SetRedHeader(red_packet, red_packet->data[1] & 0x7f, kHeaderLength);
  memcpy(red_packet->data + kHeaderLength + 1, packet->data + kHeaderLength,
         packet->length - kHeaderLength);
  return red_packet;
}

// Creates a new RtpPacket with FEC payload and RED header. Does this by
// creating a new fake media RtpPacket, clears the marker bit and adds a RED
// header. Finally replaces the payload with the content of |packet->data|.
RawRtpPacket* UlpfecPacketGenerator::BuildFecRedPacket(
    const ForwardErrorCorrection::Packet* packet) {
  // Create a fake media packet to get a correct header. 1 byte RED header.
  ++num_packets_;
  RawRtpPacket* red_packet = NextPacket(0, packet->length + 1);
  red_packet->data[1] &= ~0x80;  // Clear marker bit.
  const size_t kHeaderLength = red_packet->header.header.headerLength;
  SetRedHeader(red_packet, kFecPayloadType, kHeaderLength);
  memcpy(red_packet->data + kHeaderLength + 1, packet->data, packet->length);
  red_packet->length = kHeaderLength + 1 + packet->length;
  return red_packet;
}

void UlpfecPacketGenerator::SetRedHeader(
    ForwardErrorCorrection::Packet* red_packet,
    uint8_t payload_type,
    size_t header_length) const {
  // Replace pltype.
  red_packet->data[1] &= 0x80;             // Reset.
  red_packet->data[1] += kRedPayloadType;  // Replace.

  // Add RED header, f-bit always 0.
  red_packet->data[header_length] = payload_type;
}

void UlpfecPacketGenerator::BuildRtpHeader(uint8_t* data,
                                           const RTPHeader* header) {
  data[0] = 0x80;  // Version 2.
  data[1] = header->payloadType;
  data[1] |= (header->markerBit ? kRtpMarkerBitMask : 0);
  ByteWriter<uint16_t>::WriteBigEndian(data + 2, header->sequenceNumber);
  ByteWriter<uint32_t>::WriteBigEndian(data + 4, header->timestamp);
  ByteWriter<uint32_t>::WriteBigEndian(data + 8, header->ssrc);
}

}  // namespace fec
}  // namespace test
}  // namespace webrtc
