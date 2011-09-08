/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include "gtest/gtest.h"
#include "module_common_types.h"
#include "packet.h"
#include "session_info.h"

using webrtc::RTPFragmentationHeader;
using webrtc::RTPVideoHeaderVP8;
using webrtc::VCMPacket;
using webrtc::VCMSessionInfo;
using webrtc::WebRtcRTPHeader;

enum { kPacketBufferSize = 10 };
enum { kFrameBufferSize = 10*kPacketBufferSize };

class TestVP8MakeDecodable : public ::testing::Test {
 protected:
  virtual void SetUp() {
    memset(packet_buffer_, 0, kPacketBufferSize);
    memset(frame_buffer_, 0, kFrameBufferSize);
    vp8_header_ = &packet_header_.type.Video.codecHeader.VP8;
    packet_header_.frameType = webrtc::kVideoFrameDelta;
    packet_header_.type.Video.codec = webrtc::kRTPVideoVP8;
    vp8_header_->InitRTPVideoHeaderVP8();
    fragmentation_.VerifyAndAllocateFragmentationHeader(
        webrtc::kMaxVP8Partitions);
  }

  void FillPacket(WebRtc_UWord8 start_value) {
    for (int i = 0; i < kPacketBufferSize; ++i)
      packet_buffer_[i] = start_value + i;
  }

  bool VerifyPartition(int partition_id,
                       int packets_expected,
                       int start_value) {
    EXPECT_EQ(static_cast<WebRtc_UWord32>(packets_expected * kPacketBufferSize),
              fragmentation_.fragmentationLength[partition_id]);
    for (int i = 0; i < packets_expected; ++i) {
      int packet_index = fragmentation_.fragmentationOffset[partition_id] +
          i * kPacketBufferSize;
      for (int j = 0; j < kPacketBufferSize; ++j) {
        if (packet_index + j > kFrameBufferSize)
          return false;
        EXPECT_EQ(start_value + i + j, frame_buffer_[packet_index + j]);
      }
    }
    return true;
  }

  WebRtc_UWord8           packet_buffer_[kPacketBufferSize];
  WebRtc_UWord8           frame_buffer_[kFrameBufferSize];
  WebRtcRTPHeader         packet_header_;
  VCMSessionInfo          session_;
  RTPVideoHeaderVP8*      vp8_header_;
  RTPFragmentationHeader  fragmentation_;
};

TEST_F(TestVP8MakeDecodable, TwoPartitionsOneLoss) {
  // Partition 0 | Partition 1
  // [ 0 ] [ 2 ] | [ 3 ]
  packet_header_.type.Video.isFirstPacket = true;
  vp8_header_->beginningOfPartition = true;
  vp8_header_->partitionId = 0;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber = 0;
  FillPacket(0);
  VCMPacket* packet = new VCMPacket(packet_buffer_, kPacketBufferSize,
                                    packet_header_);
  session_.SetStartSeqNumber(packet_header_.header.sequenceNumber);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 0;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 2;
  FillPacket(2);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 1;
  vp8_header_->beginningOfPartition = true;
  packet_header_.header.markerBit = true;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(3);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  // One packet should be removed (end of partition 0).
  ASSERT_EQ(session_.BuildVP8FragmentationHeader(frame_buffer_,
                                                 kFrameBufferSize,
                                                 &fragmentation_),
            2*kPacketBufferSize);
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(0, 1, 0));
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(1, 1, 3));
}

TEST_F(TestVP8MakeDecodable, TwoPartitionsOneLoss2) {
  // Partition 0 | Partition 1
  // [ 1 ] [ 2 ] | [ 3 ] [ 5 ]
  packet_header_.type.Video.isFirstPacket = true;
  vp8_header_->beginningOfPartition = true;
  vp8_header_->partitionId = 0;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber = 1;
  FillPacket(1);
  VCMPacket* packet = new VCMPacket(packet_buffer_, kPacketBufferSize,
                                    packet_header_);
  session_.SetStartSeqNumber(packet_header_.header.sequenceNumber);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 0;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(2);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 1;
  vp8_header_->beginningOfPartition = true;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(3);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 1;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = true;
  packet_header_.header.sequenceNumber += 2;
  FillPacket(5);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  // One packet should be removed (end of partition 2), 3 left.
  ASSERT_EQ(session_.BuildVP8FragmentationHeader(frame_buffer_,
                                                 kFrameBufferSize,
                                                 &fragmentation_),
            3*kPacketBufferSize);
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(0, 2, 1));
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(1, 1, 3));
}

TEST_F(TestVP8MakeDecodable, TwoPartitionsNoLossWrap) {
  // Partition 0       | Partition 1
  // [ fffd ] [ fffe ] | [ ffff ] [ 0 ]
  packet_header_.type.Video.isFirstPacket = true;
  vp8_header_->beginningOfPartition = true;
  vp8_header_->partitionId = 0;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber = 0xfffd;
  FillPacket(0);
  VCMPacket* packet = new VCMPacket(packet_buffer_, kPacketBufferSize,
                                    packet_header_);
  session_.SetStartSeqNumber(packet_header_.header.sequenceNumber);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 0;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(1);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 1;
  vp8_header_->beginningOfPartition = true;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(2);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 1;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = true;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(3);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  // No packet should be removed.
  ASSERT_EQ(session_.BuildVP8FragmentationHeader(frame_buffer_,
                                                 kFrameBufferSize,
                                                 &fragmentation_),
            4*kPacketBufferSize);
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(0, 2, 0));
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(1, 2, 2));
}

TEST_F(TestVP8MakeDecodable, TwoPartitionsLossWrap) {
  // Partition 0       | Partition 1
  // [ fffd ] [ fffe ] | [ ffff ] [ 1 ]
  packet_header_.type.Video.isFirstPacket = true;
  vp8_header_->beginningOfPartition = true;
  vp8_header_->partitionId = 0;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber = 0xfffd;
  FillPacket(0);
  VCMPacket* packet = new VCMPacket(packet_buffer_, kPacketBufferSize,
                                    packet_header_);
  session_.SetStartSeqNumber(packet_header_.header.sequenceNumber);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 0;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(1);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 1;
  vp8_header_->beginningOfPartition = true;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(2);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 1;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = true;
  packet_header_.header.sequenceNumber += 2;
  FillPacket(3);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  // One packet should be removed from the last partition
  ASSERT_EQ(session_.BuildVP8FragmentationHeader(frame_buffer_,
                                                 kFrameBufferSize,
                                                 &fragmentation_),
            3*kPacketBufferSize);
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(0, 2, 0));
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(1, 1, 2));
}


TEST_F(TestVP8MakeDecodable, ThreePartitionsOneMissing) {
  // Partition 1  |Partition 2    | Partition 3
  // [ 1 ] [ 2 ]  |               | [ 5 ] | [ 6 ]
  packet_header_.type.Video.isFirstPacket = true;
  vp8_header_->beginningOfPartition = true;
  vp8_header_->partitionId = 0;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber = 1;
  FillPacket(1);
  VCMPacket* packet = new VCMPacket(packet_buffer_, kPacketBufferSize,
                                    packet_header_);
  session_.SetStartSeqNumber(packet_header_.header.sequenceNumber);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 0;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(2);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 2;
  vp8_header_->beginningOfPartition = true;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 3;
  FillPacket(5);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 2;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = true;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(6);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  // No packet should be removed.
  ASSERT_EQ(session_.BuildVP8FragmentationHeader(frame_buffer_,
                                                 kFrameBufferSize,
                                                 &fragmentation_),
            4*kPacketBufferSize);
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(0, 2, 1));
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(2, 2, 5));
}

TEST_F(TestVP8MakeDecodable, ThreePartitionsLossInSecond) {
  // Partition 0  |Partition 1          | Partition 2
  // [ 1 ] [ 2 ]  |        [ 4 ] [ 5 ]  | [ 6 ] [ 7 ]
  packet_header_.type.Video.isFirstPacket = true;
  vp8_header_->beginningOfPartition = true;
  vp8_header_->partitionId = 0;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber = 1;
  FillPacket(1);
  VCMPacket* packet = new VCMPacket(packet_buffer_, kPacketBufferSize,
                                    packet_header_);
  session_.SetStartSeqNumber(packet_header_.header.sequenceNumber);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 0;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(2);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 1;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 2;
  FillPacket(4);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 1;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(5);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 2;
  vp8_header_->beginningOfPartition = true;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(6);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 2;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = true;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(7);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_), kPacketBufferSize);
  delete packet;

  // 2 partitions left. 2 packets removed from second partition
  ASSERT_EQ(session_.BuildVP8FragmentationHeader(frame_buffer_,
                                                 kFrameBufferSize,
                                                 &fragmentation_),
            4*kPacketBufferSize);
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(0, 2, 1));
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(2, 2, 6));
}
