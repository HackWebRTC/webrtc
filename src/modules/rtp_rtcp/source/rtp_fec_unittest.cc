/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/forward_error_correction.h"

#include <gtest/gtest.h>
#include <list>

#include "rtp_utility.h"

using webrtc::ForwardErrorCorrection;

// Minimum RTP header size in bytes.
const uint8_t kRtpHeaderSize = 12;

// Transport header size in bytes. Assume UDP/IPv4 as a reasonable minimum.
const uint8_t kTransportOverhead = 28;

// Maximum number of media packets used in the FEC (RFC 5109).
const uint8_t kMaxNumberMediaPackets = ForwardErrorCorrection::kMaxMediaPackets;

template<typename T> void ClearList(std::list<T*>* my_list) {
  T* packet = NULL;
  while (!my_list->empty()) {
    packet = my_list->front();
    delete packet;
    my_list->pop_front();
  }
}

class RtpFecTest : public ::testing::Test {
 protected:
  RtpFecTest()
      :  fec_(new ForwardErrorCorrection(0)),
         ssrc_(rand()),
         fec_seq_num_(0) {
  }

  ForwardErrorCorrection* fec_;
  int ssrc_;
  uint16_t fec_seq_num_;

  std::list<ForwardErrorCorrection::Packet*> media_packet_list_;
  std::list<ForwardErrorCorrection::Packet*> fec_packet_list_;
  std::list<ForwardErrorCorrection::ReceivedPacket*> received_packet_list_;
  std::list<ForwardErrorCorrection::RecoveredPacket*> recovered_packet_list_;

  // Media packet "i" is lost if media_loss_mask_[i] = 1,
  // received if media_loss_mask_[i] = 0.
  int media_loss_mask_[kMaxNumberMediaPackets];

  // FEC packet "i" is lost if fec_loss_mask_[i] = 1,
  // received if fec_loss_mask_[i] = 0.
  int fec_loss_mask_[kMaxNumberMediaPackets];

  // Construct the media packet list, up to |num_media_packets| packets.
  // Returns the next sequence number after the last media packet.
  // (this will be the sequence of the first FEC packet)
  int ConstructMediaPackets(int num_media_packets);

  // Construct the received packet list: a subset of the media and FEC packets.
  void NetworkReceivedPackets();

  // Add packet from |packet_list| to list of received packets, using the
  // |loss_mask|.
  // The |packet_list| may be a media packet list (is_fec = false), or a
  // FEC packet list (is_fec = true).
  void ReceivedPackets(
      const std::list<ForwardErrorCorrection::Packet*>& packet_list,
      int* loss_mask,
      bool is_fec);

  // Check for complete recovery after FEC decoding.
  bool IsRecoveryComplete();

  // Delete the received packets.
  void FreeRecoveredPacketList();

  // Delete the media and FEC packets.
  void TearDown();
};

TEST_F(RtpFecTest, HandleIncorrectInputs) {
  int num_important_packets = 0;
  bool use_unequal_protection =  false;
  uint8_t protection_factor = 60;

  // Media packet list is empty.
  EXPECT_EQ(-1, fec_->GenerateFEC(media_packet_list_,
                                  protection_factor,
                                  num_important_packets,
                                  use_unequal_protection,
                                  &fec_packet_list_));

  int num_media_packets = 10;
  ConstructMediaPackets(num_media_packets);

  num_important_packets = -1;
  // Number of important packets below 0.
  EXPECT_EQ(-1, fec_->GenerateFEC(media_packet_list_,
                                  protection_factor,
                                  num_important_packets,
                                  use_unequal_protection,
                                  &fec_packet_list_));

  num_important_packets = 12;
  // Number of important packets greater than number of media packets.
  EXPECT_EQ(-1, fec_->GenerateFEC(media_packet_list_,
                                  protection_factor,
                                  num_important_packets,
                                  use_unequal_protection,
                                  &fec_packet_list_));

  num_media_packets = kMaxNumberMediaPackets + 1;
  ConstructMediaPackets(num_media_packets);

  num_important_packets = 0;
  // Number of media packet is above maximum allowed (kMaxNumberMediaPackets).
  EXPECT_EQ(-1, fec_->GenerateFEC(media_packet_list_,
                                  protection_factor,
                                  num_important_packets,
                                  use_unequal_protection,
                                  &fec_packet_list_));
}

TEST_F(RtpFecTest, FecRecoveryNoLoss) {
  bool frame_complete = true;
  const int num_important_packets = 0;
  const bool use_unequal_protection =  false;
  const int num_media_packets = 4;
  uint8_t protection_factor = 60;

  fec_seq_num_ = ConstructMediaPackets(num_media_packets);

  EXPECT_EQ(0, fec_->GenerateFEC(media_packet_list_,
                                 protection_factor,
                                 num_important_packets,
                                 use_unequal_protection,
                                 &fec_packet_list_));

  // Expect 1 FEC packet.
  EXPECT_EQ(1, static_cast<int>(fec_packet_list_.size()));

  // No packets lost.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_ , &recovered_packet_list_,
                               fec_seq_num_, frame_complete));

  // No packets lost, expect complete recovery.
  EXPECT_TRUE(IsRecoveryComplete());
}

TEST_F(RtpFecTest, FecRecoveryWithLoss) {
  bool frame_complete = true;
  const int num_important_packets = 0;
  const bool use_unequal_protection = false;
  const int num_media_packets = 4;
  uint8_t protection_factor = 60;

  fec_seq_num_ = ConstructMediaPackets(num_media_packets);

  EXPECT_EQ(0, fec_->GenerateFEC(media_packet_list_,
                                 protection_factor,
                                 num_important_packets,
                                 use_unequal_protection,
                                 &fec_packet_list_));

  // Expect 1 FEC packet.
  EXPECT_EQ(1, static_cast<int>(fec_packet_list_.size()));

  // 1 media packet lost
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  media_loss_mask_[3] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_ , &recovered_packet_list_,
                               fec_seq_num_, frame_complete));

  // One packet lost, one FEC packet, expect complete recovery.
  EXPECT_TRUE(IsRecoveryComplete());
  FreeRecoveredPacketList();

  // 2 media packets lost.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  media_loss_mask_[1] = 1;
  media_loss_mask_[3] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_ , &recovered_packet_list_,
                               fec_seq_num_, frame_complete));

  // 2 packets lost, one FEC packet, cannot get complete recovery.
  EXPECT_FALSE(IsRecoveryComplete());
}

TEST_F(RtpFecTest, FecRecoveryWithLoss50perc) {
  bool frame_complete = true;
  const int num_important_packets = 0;
  const bool use_unequal_protection =  false;
  const int num_media_packets = 4;
  const uint8_t protection_factor = 255;

  // Packet Mask for (4,4,0) code:
  // (num_media_packets = 4; num_fec_packets = 4, num_important_packets = 0)

  //         media#0   media#1  media#2    media#3
  // fec#0:    1          1        0          0
  // fec#1:    1          0        1          0
  // fec#2:    0          1        0          1
  // fec#3:    0          0        1          1
  //

  fec_seq_num_ = ConstructMediaPackets(num_media_packets);

  EXPECT_EQ(0, fec_->GenerateFEC(media_packet_list_,
                                 protection_factor,
                                 num_important_packets,
                                 use_unequal_protection,
                                 &fec_packet_list_));

  // Expect 4 FEC packets.
  EXPECT_EQ(4, static_cast<int>(fec_packet_list_.size()));

  // 4 packets lost: 3 media packets and one FEC packet#2 lost.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  fec_loss_mask_[2] = 1;
  media_loss_mask_[0] = 1;
  media_loss_mask_[2] = 1;
  media_loss_mask_[3] = 1;

  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_ , &recovered_packet_list_,
                               fec_seq_num_, frame_complete));

  // With media packet#1 and FEC packets #0, #1, #3, expect complete recovery.
  EXPECT_TRUE(IsRecoveryComplete());
  FreeRecoveredPacketList();

  // 4 packets lost: all media packets
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 1, sizeof(fec_loss_mask_));
  media_loss_mask_[0] = 1;
  media_loss_mask_[1] = 1;
  media_loss_mask_[2] = 1;
  media_loss_mask_[3] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_ , &recovered_packet_list_,
                               fec_seq_num_, frame_complete));

  // Cannot get complete recovery for this loss configuration.
  EXPECT_FALSE(IsRecoveryComplete());
}

TEST_F(RtpFecTest, FecRecoveryNoLossUep) {
  bool frame_complete = true;
  const int num_important_packets = 2;
  const bool use_unequal_protection =  true;
  const int num_media_packets = 4;
  const uint8_t protection_factor = 60;

  fec_seq_num_ = ConstructMediaPackets(num_media_packets);

  EXPECT_EQ(0, fec_->GenerateFEC(media_packet_list_,
                                 protection_factor,
                                 num_important_packets,
                                 use_unequal_protection,
                                 &fec_packet_list_));

  // Expect 1 FEC packet.
  EXPECT_EQ(1, static_cast<int>(fec_packet_list_.size()));

  // No packets lost.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_ , &recovered_packet_list_,
                               fec_seq_num_, frame_complete));

  // No packets lost, expect complete recovery.
  EXPECT_TRUE(IsRecoveryComplete());
}

TEST_F(RtpFecTest, FecRecoveryWithLossUep) {
  bool frame_complete = true;
  const int num_important_packets = 2;
  const bool use_unequal_protection =  true;
  const int num_media_packets = 4;
  const uint8_t protection_factor = 60;

  fec_seq_num_ = ConstructMediaPackets(num_media_packets);

  EXPECT_EQ(0, fec_->GenerateFEC(media_packet_list_,
                                 protection_factor,
                                 num_important_packets,
                                 use_unequal_protection,
                                 &fec_packet_list_));

  // Expect 1 FEC packet.
  EXPECT_EQ(1, static_cast<int>(fec_packet_list_.size()));

  // 1 media packet lost.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  media_loss_mask_[3] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_ , &recovered_packet_list_,
                               fec_seq_num_, frame_complete));

  // One packet lost, one FEC packet, expect complete recovery.
  EXPECT_TRUE(IsRecoveryComplete());
  FreeRecoveredPacketList();

  // 2 media packets lost.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  media_loss_mask_[1] = 1;
  media_loss_mask_[3] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_ , &recovered_packet_list_,
                               fec_seq_num_, frame_complete));

  // 2 packets lost, one FEC packet, cannot get complete recovery.
  EXPECT_FALSE(IsRecoveryComplete());
}

TEST_F(RtpFecTest, FecRecoveryWithLoss50percUep) {
  bool frame_complete = true;
  const int num_important_packets = 1;
  const bool use_unequal_protection =  true;
  const int num_media_packets = 4;
  const uint8_t protection_factor = 255;

  // Packet Mask for (4,4,1) code:
  // (num_media_packets = 4; num_fec_packets = 4, num_important_packets = 2)

  //         media#0   media#1  media#2    media#3
  // fec#0:    1          0        0          0
  // fec#1:    1          1        0          0
  // fec#2:    1          0        1          1
  // fec#3:    0          1        1          0
  //

  fec_seq_num_ = ConstructMediaPackets(num_media_packets);

  EXPECT_EQ(0, fec_->GenerateFEC(media_packet_list_,
                                 protection_factor,
                                 num_important_packets,
                                 use_unequal_protection,
                                 &fec_packet_list_));

  // Expect 4 FEC packets.
  EXPECT_EQ(4, static_cast<int>(fec_packet_list_.size()));

  // 4 packets lost: 3 media packets and FEC packet#1 lost.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  fec_loss_mask_[1] = 1;
  media_loss_mask_[0] = 1;
  media_loss_mask_[2] = 1;
  media_loss_mask_[3] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_ , &recovered_packet_list_,
                               fec_seq_num_, frame_complete));

  // With media packet#1 and FEC packets #0, #2, #3, expect complete recovery.
  EXPECT_TRUE(IsRecoveryComplete());
  FreeRecoveredPacketList();

  // 4 packets lost: 3 media packets and one FEC packet#2 lost.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  fec_loss_mask_[2] = 1;
  media_loss_mask_[0] = 1;
  media_loss_mask_[2] = 1;
  media_loss_mask_[3] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_ , &recovered_packet_list_,
                               fec_seq_num_, frame_complete));

  // Cannot get complete recovery for this loss configuration.
  EXPECT_FALSE(IsRecoveryComplete());
}

// TODO(marpan): Add more test cases.

void RtpFecTest::TearDown() {
  FreeRecoveredPacketList();
  ClearList(&media_packet_list_);
  EXPECT_TRUE(media_packet_list_.empty());

  fec_packet_list_.clear();
}

void RtpFecTest::FreeRecoveredPacketList() {
  ClearList(&recovered_packet_list_);
}

bool RtpFecTest::IsRecoveryComplete() {
  // Check that the number of media and recovered packets are equal.
  if (media_packet_list_.size() != recovered_packet_list_.size()) {
    return false;
  }

  ForwardErrorCorrection::Packet* media_packet;
  ForwardErrorCorrection::RecoveredPacket* recovered_packet;

  bool recovery = true;

  std::list<ForwardErrorCorrection::Packet*>::iterator
    media_packet_list_item = media_packet_list_.begin();
  std::list<ForwardErrorCorrection::RecoveredPacket*>::iterator
    recovered_packet_list_item = recovered_packet_list_.begin();
  while (media_packet_list_item != media_packet_list_.end()) {
    if (recovered_packet_list_item == recovered_packet_list_.end()) {
      return false;
    }
    media_packet = *media_packet_list_item;
    recovered_packet = *recovered_packet_list_item;
    if (recovered_packet->pkt->length != media_packet->length) {
      return false;
    }
    if (memcmp(recovered_packet->pkt->data, media_packet->data,
               media_packet->length) != 0) {
      return false;
    }
    media_packet_list_item++;
    recovered_packet_list_item++;
  }
  return recovery;
}

void RtpFecTest::NetworkReceivedPackets() {
  const bool kFecPacket = true;
  ReceivedPackets(media_packet_list_, media_loss_mask_, !kFecPacket);
  ReceivedPackets(fec_packet_list_, fec_loss_mask_, kFecPacket);
}

void RtpFecTest:: ReceivedPackets(
    const std::list<ForwardErrorCorrection::Packet*>& packet_list,
    int* loss_mask,
    bool is_fec) {
  ForwardErrorCorrection::Packet* packet;
  ForwardErrorCorrection::ReceivedPacket* received_packet;
  int seq_num = fec_seq_num_;
  int packet_idx = 0;

  std::list<ForwardErrorCorrection::Packet*>::const_iterator
  packet_list_item = packet_list.begin();

  while (packet_list_item != packet_list.end()) {
    packet = *packet_list_item;
    if (loss_mask[packet_idx] == 0) {
      received_packet = new ForwardErrorCorrection::ReceivedPacket;
      received_packet->pkt = new ForwardErrorCorrection::Packet;
      received_packet_list_.push_back(received_packet);
      received_packet->pkt->length = packet->length;
      memcpy(received_packet->pkt->data, packet->data,
             packet->length);
      received_packet->isFec = is_fec;
      if (!is_fec) {
        // For media packets, the sequence number and marker bit is
        // obtained from RTP header. These were set in ConstructMediaPackets().
        received_packet->seqNum =
            webrtc::ModuleRTPUtility::BufferToUWord16(&packet->data[2]);
        received_packet->lastMediaPktInFrame = (packet->data[1] & 0x80) != 0;
      }
      else {
        // The sequence number, marker bit, and ssrc number are defined in the
        // RTP header of the FEC packet, which is not constructed in this test.
        // So we set these values below based on the values generated in
        // ConstructMediaPackets().

        received_packet->seqNum = seq_num;
        // The marker bit (last media packet of frame) for FEC packets is
        // always zero.
        received_packet->lastMediaPktInFrame = false;
        // The ssrc value for FEC packets is set to the one used for the
        // media packets in ConstructMediaPackets().
        received_packet->ssrc = ssrc_;
      }
    }
    packet_idx++;
    packet_list_item ++;
    // Sequence number of FEC packets are defined as increment by 1 from
    // last media packet in frame.
    if (is_fec) seq_num++;
  }
}

int RtpFecTest::ConstructMediaPackets(int num_media_packets) {
  assert(num_media_packets > 0);
  ForwardErrorCorrection::Packet* media_packet = NULL;
  int sequence_number = rand();
  int time_stamp = rand();

  for (int i = 0; i < num_media_packets; i++) {
    media_packet = new ForwardErrorCorrection::Packet;
    media_packet_list_.push_back(media_packet);
    media_packet->length =
        static_cast<uint16_t>((static_cast<float>(rand()) / RAND_MAX) *
        (IP_PACKET_SIZE - kRtpHeaderSize - kTransportOverhead -
            ForwardErrorCorrection::PacketOverhead()));

    if (media_packet->length < kRtpHeaderSize) {
      media_packet->length = kRtpHeaderSize;
    }
    // Generate random values for the first 2 bytes
    media_packet->data[0] = static_cast<uint8_t>(rand() % 256);
    media_packet->data[1] = static_cast<uint8_t>(rand() % 256);

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

    webrtc::ModuleRTPUtility::AssignUWord16ToBuffer(&media_packet->data[2],
                                                    sequence_number);
    webrtc::ModuleRTPUtility::AssignUWord32ToBuffer(&media_packet->data[4],
                                                    time_stamp);
    webrtc::ModuleRTPUtility::AssignUWord32ToBuffer(&media_packet->data[8],
                                                    ssrc_);

    // Generate random values for payload.
    for (int j = 12; j < media_packet->length; j++) {
      media_packet->data[j] = static_cast<uint8_t> (rand() % 256);
    }
    sequence_number++;
  }
  // Last packet, set marker bit.
  assert(media_packet != NULL);
  media_packet->data[1] |= 0x80;
  return sequence_number;
}
