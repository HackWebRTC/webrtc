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
#include <map>
#include <set>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/random.h"
#include "webrtc/modules/video_coding/frame_object.h"
#include "webrtc/modules/video_coding/packet_buffer.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {
namespace video_coding {

class TestPacketBuffer : public ::testing::Test,
                         public OnReceivedFrameCallback {
 protected:
  TestPacketBuffer()
      : rand_(0x7732213),
        clock_(new SimulatedClock(0)),
        packet_buffer_(
            PacketBuffer::Create(clock_.get(), kStartSize, kMaxSize, this)) {}

  uint16_t Rand() { return rand_.Rand<uint16_t>(); }

  void OnReceivedFrame(std::unique_ptr<RtpFrameObject> frame) override {
    uint16_t first_seq_num = frame->first_seq_num();
    if (frames_from_callback_.find(first_seq_num) !=
        frames_from_callback_.end()) {
      ADD_FAILURE() << "Already received frame with first sequence number "
                    << first_seq_num << ".";
      return;
    }
    frames_from_callback_.insert(
        std::make_pair(frame->first_seq_num(), std::move(frame)));
  }

  enum IsKeyFrame { kKeyFrame, kDeltaFrame };
  enum IsFirst { kFirst, kNotFirst };
  enum IsLast { kLast, kNotLast };

  void InsertPacket(uint16_t seq_num,           // packet sequence number
                    IsKeyFrame keyframe,        // is keyframe
                    IsFirst first,              // is first packet of frame
                    IsLast last,                // is last packet of frame
                    int data_size = 0,          // size of data
                    uint8_t* data = nullptr) {  // data pointer
    VCMPacket packet;
    packet.codec = kVideoCodecGeneric;
    packet.seqNum = seq_num;
    packet.frameType = keyframe ? kVideoFrameKey : kVideoFrameDelta;
    packet.isFirstPacket = first == kFirst;
    packet.markerBit = last == kLast;
    packet.sizeBytes = data_size;
    packet.dataPtr = data;

    EXPECT_TRUE(packet_buffer_->InsertPacket(packet));
  }

  void CheckFrame(uint16_t first_seq_num) {
    auto frame_it = frames_from_callback_.find(first_seq_num);
    ASSERT_FALSE(frame_it == frames_from_callback_.end())
        << "Could not find frame with first sequence number " << first_seq_num
        << ".";
  }

  const int kStartSize = 16;
  const int kMaxSize = 64;

  Random rand_;
  std::unique_ptr<Clock> clock_;
  rtc::scoped_refptr<PacketBuffer> packet_buffer_;
  std::map<uint16_t, std::unique_ptr<RtpFrameObject>> frames_from_callback_;
};

TEST_F(TestPacketBuffer, InsertOnePacket) {
  uint16_t seq_num = Rand();
  InsertPacket(seq_num, kKeyFrame, kFirst, kLast);
}

TEST_F(TestPacketBuffer, InsertMultiplePackets) {
  uint16_t seq_num = Rand();
  InsertPacket(seq_num, kKeyFrame, kFirst, kLast);
  InsertPacket(seq_num + 1, kKeyFrame, kFirst, kLast);
  InsertPacket(seq_num + 2, kKeyFrame, kFirst, kLast);
  InsertPacket(seq_num + 3, kKeyFrame, kFirst, kLast);
}

TEST_F(TestPacketBuffer, InsertDuplicatePacket) {
  uint16_t seq_num = Rand();
  InsertPacket(seq_num, kKeyFrame, kFirst, kLast);
  InsertPacket(seq_num, kKeyFrame, kFirst, kLast);
}

TEST_F(TestPacketBuffer, NackCount) {
  uint16_t seq_num = Rand();

  VCMPacket packet;
  packet.codec = kVideoCodecGeneric;
  packet.seqNum = seq_num;
  packet.frameType = kVideoFrameKey;
  packet.isFirstPacket = true;
  packet.markerBit = false;
  packet.timesNacked = 0;

  packet_buffer_->InsertPacket(packet);

  packet.seqNum++;
  packet.isFirstPacket = false;
  packet.timesNacked = 1;
  packet_buffer_->InsertPacket(packet);

  packet.seqNum++;
  packet.timesNacked = 3;
  packet_buffer_->InsertPacket(packet);

  packet.seqNum++;
  packet.markerBit = true;
  packet.timesNacked = 1;
  packet_buffer_->InsertPacket(packet);


  ASSERT_EQ(1UL, frames_from_callback_.size());
  RtpFrameObject* frame = frames_from_callback_.begin()->second.get();
  EXPECT_EQ(3, frame->times_nacked());
}

TEST_F(TestPacketBuffer, FrameSize) {
  uint16_t seq_num = Rand();
  uint8_t data[] = {1, 2, 3, 4, 5};

  InsertPacket(seq_num, kKeyFrame, kFirst, kNotLast, 5, data);
  InsertPacket(seq_num + 1, kKeyFrame, kNotFirst, kNotLast, 5, data);
  InsertPacket(seq_num + 2, kKeyFrame, kNotFirst, kNotLast, 5, data);
  InsertPacket(seq_num + 3, kKeyFrame, kNotFirst, kLast, 5, data);

  ASSERT_EQ(1UL, frames_from_callback_.size());
  EXPECT_EQ(20UL, frames_from_callback_.begin()->second->size);
}

TEST_F(TestPacketBuffer, ExpandBuffer) {
  uint16_t seq_num = Rand();

  for (int i = 0; i < kStartSize + 1; ++i) {
    InsertPacket(seq_num + i, kKeyFrame, kFirst, kLast);
  }
}

TEST_F(TestPacketBuffer, ExpandBufferOverflow) {
  uint16_t seq_num = Rand();

  for (int i = 0; i < kMaxSize; ++i) {
    InsertPacket(seq_num + i, kKeyFrame, kFirst, kLast);
  }

  VCMPacket packet;
  packet.seqNum = seq_num + kMaxSize + 1;
  EXPECT_FALSE(packet_buffer_->InsertPacket(packet));
}

TEST_F(TestPacketBuffer, OnePacketOneFrame) {
  uint16_t seq_num = Rand();
  InsertPacket(seq_num, kKeyFrame, kFirst, kLast);
  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num);
}

TEST_F(TestPacketBuffer, TwoPacketsTwoFrames) {
  uint16_t seq_num = Rand();

  InsertPacket(seq_num, kKeyFrame, kFirst, kLast);
  InsertPacket(seq_num + 1, kKeyFrame, kFirst, kLast);

  EXPECT_EQ(2UL, frames_from_callback_.size());
  CheckFrame(seq_num);
  CheckFrame(seq_num + 1);
}

TEST_F(TestPacketBuffer, TwoPacketsOneFrames) {
  uint16_t seq_num = Rand();

  InsertPacket(seq_num, kKeyFrame, kFirst, kNotLast);
  InsertPacket(seq_num + 1, kKeyFrame, kNotFirst, kLast);

  EXPECT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num);
}

TEST_F(TestPacketBuffer, ThreePacketReorderingOneFrame) {
  uint16_t seq_num = Rand();

  InsertPacket(seq_num, kKeyFrame, kFirst, kNotLast);
  InsertPacket(seq_num + 2, kKeyFrame, kNotFirst, kLast);
  InsertPacket(seq_num + 1, kKeyFrame, kNotFirst, kNotLast);

  EXPECT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num);
}

TEST_F(TestPacketBuffer, DiscardOldPacket) {
  VCMPacket packet;
  packet.seqNum = Rand();
  EXPECT_TRUE(packet_buffer_->InsertPacket(packet));
  packet.seqNum += 2;
  EXPECT_TRUE(packet_buffer_->InsertPacket(packet));

  for (int i = 3; i < kMaxSize; ++i) {
    ++packet.seqNum;
    EXPECT_TRUE(packet_buffer_->InsertPacket(packet));
  }

  ++packet.seqNum;
  EXPECT_FALSE(packet_buffer_->InsertPacket(packet));
  packet_buffer_->ClearTo(packet.seqNum + 1);
  EXPECT_TRUE(packet_buffer_->InsertPacket(packet));
}

TEST_F(TestPacketBuffer, DiscardMultipleOldPackets) {
  uint16_t seq_num = Rand();
  VCMPacket packet;
  packet.seqNum = seq_num;
  EXPECT_TRUE(packet_buffer_->InsertPacket(packet));
  packet.seqNum += 2;
  EXPECT_TRUE(packet_buffer_->InsertPacket(packet));

  for (int i = 3; i < kMaxSize; ++i) {
    ++packet.seqNum;
    EXPECT_TRUE(packet_buffer_->InsertPacket(packet));
  }

  packet_buffer_->ClearTo(seq_num + 15);
  for (int i = 0; i < 15; ++i) {
    ++packet.seqNum;
    EXPECT_TRUE(packet_buffer_->InsertPacket(packet));
  }
  for (int i = 15; i < kMaxSize; ++i) {
    ++packet.seqNum;
    EXPECT_FALSE(packet_buffer_->InsertPacket(packet));
  }
}

TEST_F(TestPacketBuffer, Frames) {
  uint16_t seq_num = Rand();

  InsertPacket(seq_num, kKeyFrame, kFirst, kLast);
  InsertPacket(seq_num + 1, kDeltaFrame, kFirst, kLast);
  InsertPacket(seq_num + 2, kDeltaFrame, kFirst, kLast);
  InsertPacket(seq_num + 3, kDeltaFrame, kFirst, kLast);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckFrame(seq_num);
  CheckFrame(seq_num + 1);
  CheckFrame(seq_num + 2);
  CheckFrame(seq_num + 3);
}

TEST_F(TestPacketBuffer, FramesReordered) {
  uint16_t seq_num = Rand();

  InsertPacket(seq_num + 1, kDeltaFrame, kFirst, kLast);
  InsertPacket(seq_num, kKeyFrame, kFirst, kLast);
  InsertPacket(seq_num + 3, kDeltaFrame, kFirst, kLast);
  InsertPacket(seq_num + 2, kDeltaFrame, kFirst, kLast);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckFrame(seq_num);
  CheckFrame(seq_num + 1);
  CheckFrame(seq_num + 2);
  CheckFrame(seq_num + 3);
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

  uint16_t seq_num = Rand();

  InsertPacket(seq_num, kKeyFrame, kFirst, kNotLast, sizeof(many), many);
  InsertPacket(seq_num + 1, kDeltaFrame, kNotFirst, kNotLast, sizeof(bitstream),
               bitstream);
  InsertPacket(seq_num + 2, kDeltaFrame, kNotFirst, kNotLast, sizeof(such),
               such);
  InsertPacket(seq_num + 3, kDeltaFrame, kNotFirst, kLast, sizeof(data), data);

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num);
  EXPECT_TRUE(frames_from_callback_[seq_num]->GetBitstream(result));
  EXPECT_EQ(memcmp(result, "many bitstream, such data", sizeof(result)), 0);
}

TEST_F(TestPacketBuffer, FreeSlotsOnFrameDestruction) {
  uint16_t seq_num = Rand();

  InsertPacket(seq_num, kKeyFrame, kFirst, kNotLast);
  InsertPacket(seq_num + 1, kDeltaFrame, kNotFirst, kNotLast);
  InsertPacket(seq_num + 2, kDeltaFrame, kNotFirst, kLast);
  EXPECT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num);

  frames_from_callback_.clear();

  // Insert frame that fills the whole buffer.
  InsertPacket(seq_num + 3, kKeyFrame, kFirst, kNotLast);
  for (int i = 0; i < kMaxSize - 2; ++i)
    InsertPacket(seq_num + i + 4, kDeltaFrame, kNotFirst, kNotLast);
  InsertPacket(seq_num + kMaxSize + 2, kKeyFrame, kNotFirst, kLast);
  EXPECT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num + 3);
}

TEST_F(TestPacketBuffer, Clear) {
  uint16_t seq_num = Rand();

  InsertPacket(seq_num, kKeyFrame, kFirst, kNotLast);
  InsertPacket(seq_num + 1, kDeltaFrame, kNotFirst, kNotLast);
  InsertPacket(seq_num + 2, kDeltaFrame, kNotFirst, kLast);
  EXPECT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num);

  packet_buffer_->Clear();

  InsertPacket(seq_num + kStartSize, kKeyFrame, kFirst, kNotLast);
  InsertPacket(seq_num + kStartSize + 1, kDeltaFrame, kNotFirst, kNotLast);
  InsertPacket(seq_num + kStartSize + 2, kDeltaFrame, kNotFirst, kLast);
  EXPECT_EQ(2UL, frames_from_callback_.size());
  CheckFrame(seq_num + kStartSize);
}

TEST_F(TestPacketBuffer, InvalidateFrameByClearing) {
  VCMPacket packet;
  packet.frameType = kVideoFrameKey;
  packet.isFirstPacket = true;
  packet.markerBit = true;
  packet.seqNum = Rand();
  EXPECT_TRUE(packet_buffer_->InsertPacket(packet));
  ASSERT_EQ(1UL, frames_from_callback_.size());

  packet_buffer_->Clear();
  EXPECT_FALSE(frames_from_callback_.begin()->second->GetBitstream(nullptr));
}

}  // namespace video_coding
}  // namespace webrtc
