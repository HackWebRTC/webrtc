/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstring>
#include <limits>

#include "webrtc/modules/video_coding/frame_object.h"
#include "webrtc/modules/video_coding/packet_buffer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/random.h"

namespace webrtc {
namespace video_coding {

class TestPacketBuffer : public ::testing::Test,
                         public OnCompleteFrameCallback {
 protected:
  TestPacketBuffer()
      : rand_(0x8739211), packet_buffer_(kStartSize, kMaxSize, this) {}

  uint16_t Rand() { return rand_.Rand(std::numeric_limits<uint16_t>::max()); }

  void OnCompleteFrame(std::unique_ptr<FrameObject> frame) override {
    frames_from_callback_.emplace_back(std::move(frame));
  }

  void TearDown() override {
    // All FrameObjects must be destroyed before the PacketBuffer since
    // a FrameObject will try to remove itself from the packet buffer
    // upon destruction.
    frames_from_callback_.clear();
  }

  const int kStartSize = 16;
  const int kMaxSize = 64;

  Random rand_;
  PacketBuffer packet_buffer_;
  std::vector<std::unique_ptr<FrameObject>> frames_from_callback_;
};

TEST_F(TestPacketBuffer, InsertOnePacket) {
  VCMPacket packet;
  packet.seqNum = Rand();
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
}

TEST_F(TestPacketBuffer, InsertMultiplePackets) {
  VCMPacket packet;
  packet.seqNum = Rand();
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  ++packet.seqNum;
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  ++packet.seqNum;
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
}

TEST_F(TestPacketBuffer, InsertDuplicatePacket) {
  VCMPacket packet;
  packet.seqNum = Rand();
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  ++packet.seqNum;
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  ++packet.seqNum;
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
}

TEST_F(TestPacketBuffer, ExpandBuffer) {
  VCMPacket packet;
  packet.seqNum = Rand();

  for (int i = 0; i < kStartSize + 1; ++i) {
    EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
    ++packet.seqNum;
  }
}

TEST_F(TestPacketBuffer, ExpandBufferOverflow) {
  VCMPacket packet;
  packet.seqNum = Rand();

  for (int i = 0; i < kMaxSize; ++i) {
    EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
    ++packet.seqNum;
  }

  EXPECT_FALSE(packet_buffer_.InsertPacket(packet));
}

TEST_F(TestPacketBuffer, OnePacketOneFrame) {
  VCMPacket packet;
  packet.isFirstPacket = true;
  packet.markerBit = true;
  packet.seqNum = Rand();
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  EXPECT_EQ(1UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, TwoPacketsTwoFrames) {
  VCMPacket packet;
  packet.isFirstPacket = true;
  packet.markerBit = true;
  packet.seqNum = Rand();
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  ++packet.seqNum;
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  EXPECT_EQ(2UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, TwoPacketsOneFrames) {
  VCMPacket packet;
  packet.isFirstPacket = true;
  packet.seqNum = Rand();
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  packet.markerBit = true;
  ++packet.seqNum;
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  EXPECT_EQ(1UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, ThreePacketReorderingOneFrame) {
  VCMPacket packet;
  packet.isFirstPacket = true;
  packet.seqNum = Rand();
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  EXPECT_EQ(0UL, frames_from_callback_.size());
  packet.isFirstPacket = false;
  packet.markerBit = true;
  packet.seqNum += 2;
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  EXPECT_EQ(0UL, frames_from_callback_.size());
  packet.markerBit = false;
  packet.seqNum -= 1;
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  EXPECT_EQ(1UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, IndexWrapOneFrame) {
  VCMPacket packet;
  packet.isFirstPacket = true;
  packet.seqNum = kStartSize - 1;
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  EXPECT_EQ(0UL, frames_from_callback_.size());
  packet.isFirstPacket = false;
  ++packet.seqNum;
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  EXPECT_EQ(0UL, frames_from_callback_.size());
  ++packet.seqNum;
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  EXPECT_EQ(0UL, frames_from_callback_.size());
  packet.markerBit = true;
  ++packet.seqNum;
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  EXPECT_EQ(1UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, DiscardOldPacket) {
  uint16_t seq_num = Rand();
  VCMPacket packet;
  packet.seqNum = Rand();
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  packet.seqNum += 2;
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));

  for (int i = 3; i < kMaxSize; ++i) {
    ++packet.seqNum;
    EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  }

  ++packet.seqNum;
  EXPECT_FALSE(packet_buffer_.InsertPacket(packet));
  packet_buffer_.ClearTo(seq_num + 1);
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
}

TEST_F(TestPacketBuffer, DiscardMultipleOldPackets) {
  uint16_t seq_num = Rand();
  VCMPacket packet;
  packet.seqNum = seq_num;
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  packet.seqNum += 2;
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));

  for (int i = 3; i < kMaxSize; ++i) {
    ++packet.seqNum;
    EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  }

  packet_buffer_.ClearTo(seq_num + 15);
  for (int i = 0; i < 15; ++i) {
    ++packet.seqNum;
    EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  }
  for (int i = 15; i < kMaxSize; ++i) {
    ++packet.seqNum;
    EXPECT_FALSE(packet_buffer_.InsertPacket(packet));
  }
}

TEST_F(TestPacketBuffer, GetBitstreamFromFrame) {
  // "many bitstream, such data" with null termination.
  uint8_t many[] = {0x6d, 0x61, 0x6e, 0x79, 0x20};
  uint8_t bitstream[] = {0x62, 0x69, 0x74, 0x73, 0x74, 0x72,
                         0x65, 0x61, 0x6d, 0x2c, 0x20};
  uint8_t such[] = {0x73, 0x75, 0x63, 0x68, 0x20};
  uint8_t data[] = {0x64, 0x61, 0x74, 0x61, 0x0};
  uint8_t
      result[sizeof(many) + sizeof(bitstream) + sizeof(such) + sizeof(data)];

  VCMPacket packet;
  packet.isFirstPacket = true;
  packet.seqNum = 0xfffe;
  packet.dataPtr = many;
  packet.sizeBytes = sizeof(many);
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  packet.isFirstPacket = false;
  ++packet.seqNum;
  packet.dataPtr = bitstream;
  packet.sizeBytes = sizeof(bitstream);
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  ++packet.seqNum;
  packet.dataPtr = such;
  packet.sizeBytes = sizeof(such);
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  packet.markerBit = true;
  ++packet.seqNum;
  packet.dataPtr = data;
  packet.sizeBytes = sizeof(data);
  EXPECT_EQ(0UL, frames_from_callback_.size());
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  ASSERT_EQ(1UL, frames_from_callback_.size());

  EXPECT_TRUE(frames_from_callback_[0]->GetBitstream(result));
  EXPECT_EQ(
      std::strcmp("many bitstream, such data", reinterpret_cast<char*>(result)),
      0);
}

TEST_F(TestPacketBuffer, FreeSlotsOnFrameDestruction) {
  VCMPacket packet;
  packet.isFirstPacket = true;
  packet.seqNum = Rand();
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  EXPECT_EQ(0UL, frames_from_callback_.size());
  packet.isFirstPacket = false;
  ++packet.seqNum;
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  EXPECT_EQ(0UL, frames_from_callback_.size());
  ++packet.seqNum;
  packet.markerBit = true;
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  EXPECT_EQ(1UL, frames_from_callback_.size());

  frames_from_callback_.clear();

  packet.isFirstPacket = true;
  packet.markerBit = false;
  packet.seqNum = Rand();
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  EXPECT_EQ(0UL, frames_from_callback_.size());
  packet.isFirstPacket = false;
  ++packet.seqNum;
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  EXPECT_EQ(0UL, frames_from_callback_.size());
  ++packet.seqNum;
  packet.markerBit = true;
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  EXPECT_EQ(1UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, Flush) {
  VCMPacket packet;
  packet.isFirstPacket = true;
  packet.markerBit = true;
  packet.seqNum = Rand();
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  packet_buffer_.Flush();
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  EXPECT_EQ(2UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, InvalidateFrameByFlushing) {
  VCMPacket packet;
  packet.isFirstPacket = true;
  packet.markerBit = true;
  packet.seqNum = Rand();
  EXPECT_TRUE(packet_buffer_.InsertPacket(packet));
  ASSERT_EQ(1UL, frames_from_callback_.size());

  packet_buffer_.Flush();
  EXPECT_FALSE(frames_from_callback_[0]->GetBitstream(nullptr));
}

}  // namespace video_coding
}  // namespace webrtc
