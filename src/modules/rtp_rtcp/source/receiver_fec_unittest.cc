/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>
#include <list>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "modules/rtp_rtcp/source/forward_error_correction.h"
#include "modules/rtp_rtcp/source/mock/mock_rtp_receiver_video.h"
#include "modules/rtp_rtcp/source/receiver_fec.h"

using ::testing::_;
using ::testing::Args;
using ::testing::ElementsAreArray;
using ::testing::InSequence;

namespace webrtc {

typedef ForwardErrorCorrection::Packet Packet;

enum { kRtpHeaderSize = 12 };
enum { kFecPayloadType = 96 };
enum { kRedPayloadType = 97 };
enum { kVp8PayloadType = 120 };

struct RtpPacket : public Packet {
  WebRtcRTPHeader header;
};

class FrameGenerator {
 public:
  FrameGenerator() : num_packets_(0), seq_num_(0), timestamp_(0) {}

  void NewFrame(int num_packets) {
    num_packets_ = num_packets;
    timestamp_ += 3000;
  }

  RtpPacket* NextPacket(int offset, size_t length) {
    RtpPacket* rtp_packet = new RtpPacket;
    for (size_t i = 0; i < length; ++i)
      rtp_packet->data[i] = offset + i;
    rtp_packet->length = length;
    memset(&rtp_packet->header, 0, sizeof(WebRtcRTPHeader));
    rtp_packet->header.frameType = kVideoFrameDelta;
    rtp_packet->header.header.headerLength = kRtpHeaderSize;
    rtp_packet->header.header.markerBit = (num_packets_ == 1);
    rtp_packet->header.header.sequenceNumber = seq_num_;
    rtp_packet->header.header.timestamp = timestamp_;
    rtp_packet->header.header.payloadType = kVp8PayloadType;
    BuildRtpHeader(rtp_packet->data, rtp_packet->header.header);
    ++seq_num_;
    --num_packets_;
    return rtp_packet;
  }

  // Creates a new RtpPacket with the RED header added to the packet.
  RtpPacket* BuildMediaRedPacket(const RtpPacket* packet) {
    const int kHeaderLength = packet->header.header.headerLength;
    RtpPacket* red_packet = new RtpPacket;
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

  // Creates a new RtpPacket with FEC payload and red header. Does this by
  // creating a new fake media RtpPacket, clears the marker bit and adds a RED
  // header. Finally replaces the payload with the content of |packet->data|.
  RtpPacket* BuildFecRedPacket(const Packet* packet) {
    // Create a fake media packet to get a correct header. 1 byte RED header.
    ++num_packets_;
    RtpPacket* red_packet = NextPacket(0, packet->length + 1);
    red_packet->data[1] &= ~0x80;  // Clear marker bit.
    const int kHeaderLength = red_packet->header.header.headerLength;
    SetRedHeader(red_packet, kFecPayloadType, kHeaderLength);
    memcpy(red_packet->data + kHeaderLength + 1, packet->data,
           packet->length);
    red_packet->length = kHeaderLength + 1 + packet->length;
    return red_packet;
  }

  void SetRedHeader(Packet* red_packet, uint8_t payload_type,
                    int header_length) const {
    // Replace pltype.
    red_packet->data[1] &= 0x80;  // Reset.
    red_packet->data[1] += kRedPayloadType;  // Replace.

    // Add RED header, f-bit always 0.
    red_packet->data[header_length] = payload_type;
  }

 private:
  void BuildRtpHeader(uint8_t* data, RTPHeader header) {
    data[0] = 0x80;  // Version 2.
    data[1] = header.payloadType;
    data[1] |= (header.markerBit ? kRtpMarkerBitMask : 0);
    ModuleRTPUtility::AssignUWord16ToBuffer(data+2, header.sequenceNumber);
    ModuleRTPUtility::AssignUWord32ToBuffer(data+4, header.timestamp);
    ModuleRTPUtility::AssignUWord32ToBuffer(data+8, header.ssrc);
  }

  int num_packets_;
  uint16_t seq_num_;
  uint32_t timestamp_;
};

class ReceiverFecTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    fec_ = new ForwardErrorCorrection(0);
    receiver_fec_ = new ReceiverFEC(0, &rtp_receiver_video_);
    generator_ = new FrameGenerator();
  }

  virtual void TearDown() {
    delete fec_;
    delete receiver_fec_;
    delete generator_;
  }

  void GenerateFrame(int num_media_packets,
                     int frame_offset,
                     std::list<RtpPacket*>* media_rtp_packets,
                     std::list<Packet*>* media_packets) {
    generator_->NewFrame(num_media_packets);
    for (int i = 0; i < num_media_packets; ++i) {
      media_rtp_packets->push_back(generator_->NextPacket(frame_offset + i,
                                                          kRtpHeaderSize + 10));
      media_packets->push_back(media_rtp_packets->back());
    }
  }

  void VerifyReconstructedMediaPacket(const RtpPacket* packet, int times) {
    // Verify that the content of the reconstructed packet is equal to the
    // content of |packet|, and that the same content is received |times| number
    // of times in a row.
    EXPECT_CALL(rtp_receiver_video_,
                ReceiveRecoveredPacketCallback(_, _,
                                               packet->length - kRtpHeaderSize))
        .With(Args<1, 2>(ElementsAreArray(packet->data + kRtpHeaderSize,
                                          packet->length - kRtpHeaderSize)))
        .Times(times);
  }

  void BuildAndAddRedMediaPacket(RtpPacket* packet) {
    RtpPacket* red_packet = generator_->BuildMediaRedPacket(packet);
    bool is_fec = false;
    EXPECT_EQ(0, receiver_fec_->AddReceivedFECPacket(&red_packet->header,
                                                     red_packet->data,
                                                     red_packet->length -
                                                     kRtpHeaderSize,
                                                     is_fec,
                                                     false));
    delete red_packet;
    EXPECT_FALSE(is_fec);
  }

  void BuildAndAddRedFecPacket(Packet* packet) {
    RtpPacket* red_packet = generator_->BuildFecRedPacket(packet);
    bool is_fec = false;
    EXPECT_EQ(0, receiver_fec_->AddReceivedFECPacket(&red_packet->header,
                                                     red_packet->data,
                                                     red_packet->length -
                                                     kRtpHeaderSize,
                                                     is_fec,
                                                     false));
    delete red_packet;
    EXPECT_TRUE(is_fec);
  }

  ForwardErrorCorrection* fec_;
  MockRTPReceiverVideo rtp_receiver_video_;
  ReceiverFEC* receiver_fec_;
  FrameGenerator* generator_;
};

void DeletePackets(std::list<Packet*>* packets) {
  while (!packets->empty()) {
    delete packets->front();
    packets->pop_front();
  }
}

TEST_F(ReceiverFecTest, TwoMediaOneFec) {
  const unsigned int kNumFecPackets = 1u;
  const unsigned int kNumMediaPackets = 2u;
  std::list<RtpPacket*> media_rtp_packets;
  std::list<Packet*> media_packets;
  GenerateFrame(2, 0, &media_rtp_packets, &media_packets);
  std::list<Packet*> fec_packets;
  EXPECT_EQ(0, fec_->GenerateFEC(media_packets,
                                 kNumFecPackets * 255 / kNumMediaPackets,
                                 0,
                                 false,
                                 &fec_packets));
  ASSERT_EQ(kNumFecPackets, fec_packets.size());

  // Recovery
  receiver_fec_->SetPayloadTypeFEC(kFecPayloadType);
  std::list<RtpPacket*>::iterator media_it = media_rtp_packets.begin();
  BuildAndAddRedMediaPacket(*media_it);
  // Drop one media packet.
  std::list<Packet*>::iterator fec_it = fec_packets.begin();
  BuildAndAddRedFecPacket(*fec_it);
  {
    InSequence s;
    std::list<RtpPacket*>::iterator it = media_rtp_packets.begin();
    VerifyReconstructedMediaPacket(*it, 1);
    ++it;
    VerifyReconstructedMediaPacket(*it, 1);
  }
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFEC(false));

  DeletePackets(&media_packets);
}

TEST_F(ReceiverFecTest, TwoMediaTwoFec) {
  const unsigned int kNumFecPackets = 2u;
  const unsigned int kNumMediaPackets = 2u;
  std::list<RtpPacket*> media_rtp_packets;
  std::list<Packet*> media_packets;
  GenerateFrame(2, 0, &media_rtp_packets, &media_packets);
  std::list<Packet*> fec_packets;
  EXPECT_EQ(0, fec_->GenerateFEC(media_packets,
                                 kNumFecPackets * 255 / kNumMediaPackets,
                                 0,
                                 false, &fec_packets));
  ASSERT_EQ(kNumFecPackets, fec_packets.size());

  // Recovery
  // Drop both media packets.
  receiver_fec_->SetPayloadTypeFEC(kFecPayloadType);
  std::list<Packet*>::iterator fec_it = fec_packets.begin();
  BuildAndAddRedFecPacket(*fec_it);
  ++fec_it;
  BuildAndAddRedFecPacket(*fec_it);
  {
    InSequence s;
    std::list<RtpPacket*>::iterator it = media_rtp_packets.begin();
    VerifyReconstructedMediaPacket(*it, 1);
    ++it;
    VerifyReconstructedMediaPacket(*it, 1);
  }
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFEC(false));

  DeletePackets(&media_packets);
}

TEST_F(ReceiverFecTest, TwoFramesOneFec) {
  const unsigned int kNumFecPackets = 1u;
  const unsigned int kNumMediaPackets = 2u;
  std::list<RtpPacket*> media_rtp_packets;
  std::list<Packet*> media_packets;
  GenerateFrame(1, 0, &media_rtp_packets, &media_packets);
  GenerateFrame(1, 1, &media_rtp_packets, &media_packets);
  std::list<Packet*> fec_packets;
  EXPECT_EQ(0, fec_->GenerateFEC(media_packets,
                                 kNumFecPackets * 255 / kNumMediaPackets,
                                 0,
                                 false,
                                 &fec_packets));
  ASSERT_EQ(kNumFecPackets, fec_packets.size());

  // Recovery
  receiver_fec_->SetPayloadTypeFEC(kFecPayloadType);
  BuildAndAddRedMediaPacket(media_rtp_packets.front());
  // Drop one media packet.
  BuildAndAddRedFecPacket(fec_packets.front());
  {
    InSequence s;
    std::list<RtpPacket*>::iterator it = media_rtp_packets.begin();
    VerifyReconstructedMediaPacket(*it, 1);
    ++it;
    VerifyReconstructedMediaPacket(*it, 1);
  }
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFEC(false));

  DeletePackets(&media_packets);
}

TEST_F(ReceiverFecTest, MaxFramesOneFec) {
  const unsigned int kNumFecPackets = 1u;
  const unsigned int kNumMediaPackets = 48u;
  std::list<RtpPacket*> media_rtp_packets;
  std::list<Packet*> media_packets;
  for (unsigned int i = 0; i < kNumMediaPackets; ++i)
    GenerateFrame(1, i, &media_rtp_packets, &media_packets);
  std::list<Packet*> fec_packets;
  EXPECT_EQ(0, fec_->GenerateFEC(media_packets,
                                 kNumFecPackets * 255 / kNumMediaPackets,
                                 0,
                                 false,
                                 &fec_packets));
  ASSERT_EQ(kNumFecPackets, fec_packets.size());

  // Recovery
  receiver_fec_->SetPayloadTypeFEC(kFecPayloadType);
  std::list<RtpPacket*>::iterator it = media_rtp_packets.begin();
  ++it;  // Drop first packet.
  for (; it != media_rtp_packets.end(); ++it)
    BuildAndAddRedMediaPacket(*it);
  BuildAndAddRedFecPacket(fec_packets.front());
  {
    InSequence s;
    std::list<RtpPacket*>::iterator it = media_rtp_packets.begin();
    for (; it != media_rtp_packets.end(); ++it)
      VerifyReconstructedMediaPacket(*it, 1);
  }
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFEC(false));

  DeletePackets(&media_packets);
}

TEST_F(ReceiverFecTest, TooManyFrames) {
  const unsigned int kNumFecPackets = 1u;
  const unsigned int kNumMediaPackets = 49u;
  std::list<RtpPacket*> media_rtp_packets;
  std::list<Packet*> media_packets;
  for (unsigned int i = 0; i < kNumMediaPackets; ++i)
    GenerateFrame(1, i, &media_rtp_packets, &media_packets);
  std::list<Packet*> fec_packets;
  EXPECT_EQ(-1, fec_->GenerateFEC(media_packets,
                                  kNumFecPackets * 255 / kNumMediaPackets,
                                  0,
                                  false,
                                  &fec_packets));

  DeletePackets(&media_packets);
}

}  // namespace webrtc
