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
      : rand_(0x8739211),
        packet_buffer_(new PacketBuffer(kStartSize, kMaxSize, this)) {}

  uint16_t Rand() { return rand_.Rand(std::numeric_limits<uint16_t>::max()); }

  void OnCompleteFrame(std::unique_ptr<FrameObject> frame) override {
    uint16_t pid = frame->picture_id;
    auto frame_it = frames_from_callback_.find(pid);
    if (frame_it != frames_from_callback_.end()) {
      ADD_FAILURE() << "Already received frame with picture id: " << pid;
      return;
    }

    frames_from_callback_.insert(
        make_pair(frame->picture_id, std::move(frame)));
  }

  void TearDown() override {
    // All frame objects must be destroyed before the packet buffer since
    // a frame object will try to remove itself from the packet buffer
    // upon destruction.
    frames_from_callback_.clear();
  }

  // Insert a generic packet into the packet buffer.
  void InsertGeneric(uint16_t seq_num,              // packet sequence number
                     bool keyframe,                 // is keyframe
                     bool first,                    // is first packet of frame
                     bool last,                     // is last packet of frame
                     size_t data_size = 0,          // size of data
                     uint8_t* data = nullptr) {     // data pointer
    VCMPacket packet;
    packet.codec = kVideoCodecGeneric;
    packet.seqNum = seq_num;
    packet.frameType = keyframe ? kVideoFrameKey : kVideoFrameDelta;
    packet.isFirstPacket = first;
    packet.markerBit = last;
    packet.sizeBytes = data_size;
    packet.dataPtr = data;

    EXPECT_TRUE(packet_buffer_->InsertPacket(packet));
  }

  // Insert a Vp8 packet into the packet buffer.
  void InsertVp8(uint16_t seq_num,              // packet sequence number
                 bool keyframe,                 // is keyframe
                 bool first,                    // is first packet of frame
                 bool last,                     // is last packet of frame
                 bool sync = false,             // is sync frame
                 int32_t pid = kNoPictureId,    // picture id
                 uint8_t tid = kNoTemporalIdx,  // temporal id
                 int32_t tl0 = kNoTl0PicIdx,    // tl0 pic index
                 size_t data_size = 0,          // size of data
                 uint8_t* data = nullptr) {     // data pointer
    VCMPacket packet;
    packet.codec = kVideoCodecVP8;
    packet.seqNum = seq_num;
    packet.frameType = keyframe ? kVideoFrameKey : kVideoFrameDelta;
    packet.isFirstPacket = first;
    packet.markerBit = last;
    packet.sizeBytes = data_size;
    packet.dataPtr = data;
    packet.codecSpecificHeader.codecHeader.VP8.pictureId = pid % (1 << 15);
    packet.codecSpecificHeader.codecHeader.VP8.temporalIdx = tid;
    packet.codecSpecificHeader.codecHeader.VP8.tl0PicIdx = tl0;
    packet.codecSpecificHeader.codecHeader.VP8.layerSync = sync;

    EXPECT_TRUE(packet_buffer_->InsertPacket(packet));
  }

  // Check if a frame with picture id |pid| has been delivered from the packet
  // buffer, and if so, if it has the references specified by |refs|.
  template <typename... T>
  void CheckReferences(uint16_t pid, T... refs) const {
    auto frame_it = frames_from_callback_.find(pid);
    if (frame_it == frames_from_callback_.end()) {
      ADD_FAILURE() << "Could not find frame with picture id " << pid;
      return;
    }

    std::set<uint16_t> actual_refs;
    for (uint8_t r = 0; r < frame_it->second->num_references; ++r) {
      actual_refs.insert(frame_it->second->references[r]);
    }

    std::set<uint16_t> expected_refs;
    RefsToSet(&expected_refs, refs...);

    ASSERT_EQ(expected_refs, actual_refs);
  }

  template <typename... T>
  void RefsToSet(std::set<uint16_t>* m, uint16_t ref, T... refs) const {
    m->insert(ref);
    RefsToSet(m, refs...);
  }

  void RefsToSet(std::set<uint16_t>* m) const {}

  const int kStartSize = 16;
  const int kMaxSize = 64;

  Random rand_;
  std::unique_ptr<PacketBuffer> packet_buffer_;
  std::map<uint16_t, std::unique_ptr<FrameObject>> frames_from_callback_;
};

TEST_F(TestPacketBuffer, InsertOnePacket) {
  VCMPacket packet;
  packet.seqNum = Rand();
  EXPECT_TRUE(packet_buffer_->InsertPacket(packet));
}

TEST_F(TestPacketBuffer, InsertMultiplePackets) {
  VCMPacket packet;
  packet.seqNum = Rand();
  EXPECT_TRUE(packet_buffer_->InsertPacket(packet));
  ++packet.seqNum;
  EXPECT_TRUE(packet_buffer_->InsertPacket(packet));
  ++packet.seqNum;
  EXPECT_TRUE(packet_buffer_->InsertPacket(packet));
}

TEST_F(TestPacketBuffer, InsertDuplicatePacket) {
  VCMPacket packet;
  packet.seqNum = Rand();
  EXPECT_TRUE(packet_buffer_->InsertPacket(packet));
  ++packet.seqNum;
  EXPECT_TRUE(packet_buffer_->InsertPacket(packet));
  EXPECT_TRUE(packet_buffer_->InsertPacket(packet));
}

TEST_F(TestPacketBuffer, ExpandBuffer) {
  uint16_t seq_num = Rand();

  for (int i = 0; i < kStartSize + 1; ++i) {
    //            seq_num    , keyframe, first, last
    InsertGeneric(seq_num + i, true    , true , true);
  }
}

TEST_F(TestPacketBuffer, ExpandBufferOverflow) {
  uint16_t seq_num = Rand();

  for (int i = 0; i < kMaxSize; ++i) {
    //            seq_num    , keyframe, first, last
    InsertGeneric(seq_num + i, true    , true , true);
  }

  VCMPacket packet;
  packet.seqNum = seq_num + kMaxSize + 1;
  packet.sizeBytes = 1;
  EXPECT_FALSE(packet_buffer_->InsertPacket(packet));
}

TEST_F(TestPacketBuffer, GenericOnePacketOneFrame) {
  //            seq_num, keyframe, first, last
  InsertGeneric(Rand() , true    , true , true);
  ASSERT_EQ(1UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, GenericTwoPacketsTwoFrames) {
  uint16_t seq_num = Rand();

  //            seq_num    , keyframe, first, last
  InsertGeneric(seq_num    , true    , true , true);
  InsertGeneric(seq_num + 1, true    , true , true);

  EXPECT_EQ(2UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, GenericTwoPacketsOneFrames) {
  uint16_t seq_num = Rand();

  //            seq_num    , keyframe, first, last
  InsertGeneric(seq_num    , true    , true , false);
  InsertGeneric(seq_num + 1, true    , false, true);

  EXPECT_EQ(1UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, GenericThreePacketReorderingOneFrame) {
  uint16_t seq_num = Rand();

  //            seq_num    , keyframe, first, last
  InsertGeneric(seq_num    , true    , true , false);
  InsertGeneric(seq_num + 2, true    , false, true);
  InsertGeneric(seq_num + 1, true    , false, false);

  EXPECT_EQ(1UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, DiscardOldPacket) {
  uint16_t seq_num = Rand();
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
  packet_buffer_->ClearTo(seq_num + 1);
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

TEST_F(TestPacketBuffer, GenericFrames) {
  uint16_t seq_num = Rand();

  //            seq_num    , keyf , first, last
  InsertGeneric(seq_num    , true , true , true);
  InsertGeneric(seq_num + 1, false, true , true);
  InsertGeneric(seq_num + 2, false, true , true);
  InsertGeneric(seq_num + 3, false, true , true);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckReferences(seq_num);
  CheckReferences(seq_num + 1, seq_num);
  CheckReferences(seq_num + 2, seq_num + 1);
  CheckReferences(seq_num + 3, seq_num + 2);
}

TEST_F(TestPacketBuffer, GenericFramesReordered) {
  uint16_t seq_num = Rand();

  //            seq_num    , keyf , first, last
  InsertGeneric(seq_num + 1, false, true , true);
  InsertGeneric(seq_num    , true , true , true);
  InsertGeneric(seq_num + 3, false, true , true);
  InsertGeneric(seq_num + 2, false, true , true);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckReferences(seq_num);
  CheckReferences(seq_num + 1, seq_num);
  CheckReferences(seq_num + 2, seq_num + 1);
  CheckReferences(seq_num + 3, seq_num + 2);
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

  //            seq_num    , keyf , first, last , data_size        , data
  InsertGeneric(seq_num    , true , true , false, sizeof(many)     , many);
  InsertGeneric(seq_num + 1, false, false, false, sizeof(bitstream), bitstream);
  InsertGeneric(seq_num + 2, false, false, false, sizeof(such)     , such);
  InsertGeneric(seq_num + 3, false, false, true , sizeof(data)     , data);

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckReferences(seq_num + 3);
  EXPECT_TRUE(frames_from_callback_[seq_num + 3]->GetBitstream(result));
  EXPECT_EQ(std::strcmp("many bitstream, such data",
            reinterpret_cast<char*>(result)),
            0);
}

TEST_F(TestPacketBuffer, FreeSlotsOnFrameDestruction) {
  uint16_t seq_num = Rand();

  //            seq_num    , keyf , first, last
  InsertGeneric(seq_num    , true , true , false);
  InsertGeneric(seq_num + 1, false, false, false);
  InsertGeneric(seq_num + 2, false, false, true);
  EXPECT_EQ(1UL, frames_from_callback_.size());

  frames_from_callback_.clear();

  //            seq_num    , keyf , first, last
  InsertGeneric(seq_num    , true , true , false);
  InsertGeneric(seq_num + 1, false, false, false);
  InsertGeneric(seq_num + 2, false, false, true);
  EXPECT_EQ(1UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, Flush) {
  uint16_t seq_num = Rand();

  //            seq_num    , keyf , first, last
  InsertGeneric(seq_num    , true , true , false);
  InsertGeneric(seq_num + 1, false, false, false);
  InsertGeneric(seq_num + 2, false, false, true);
  EXPECT_EQ(1UL, frames_from_callback_.size());

  packet_buffer_->Flush();

  //            seq_num                 , keyf , first, last
  InsertGeneric(seq_num + kStartSize    , true , true , false);
  InsertGeneric(seq_num + kStartSize + 1, false, false, false);
  InsertGeneric(seq_num + kStartSize + 2, false, false, true);
  EXPECT_EQ(2UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, InvalidateFrameByFlushing) {
  VCMPacket packet;
  packet.codec = kVideoCodecGeneric;
  packet.frameType = kVideoFrameKey;
  packet.isFirstPacket = true;
  packet.markerBit = true;
  packet.seqNum = Rand();
  EXPECT_TRUE(packet_buffer_->InsertPacket(packet));
  ASSERT_EQ(1UL, frames_from_callback_.size());

  packet_buffer_->Flush();
  EXPECT_FALSE(frames_from_callback_.begin()->second->GetBitstream(nullptr));
}

TEST_F(TestPacketBuffer, Vp8NoPictureId) {
  uint16_t seq_num = Rand();

  //        seq_num     , keyf , first, last
  InsertVp8(seq_num     , true , true , false);
  InsertVp8(seq_num + 1 , false, false, false);
  InsertVp8(seq_num + 2 , false, false, true);
  ASSERT_EQ(1UL, frames_from_callback_.size());

  InsertVp8(seq_num + 3 , false, true , false);
  InsertVp8(seq_num + 4 , false, false, true);
  ASSERT_EQ(2UL, frames_from_callback_.size());

  InsertVp8(seq_num + 5 , false, true , false);
  InsertVp8(seq_num + 6 , false, false, false);
  InsertVp8(seq_num + 7 , false, false, false);
  InsertVp8(seq_num + 8 , false, false, true);
  ASSERT_EQ(3UL, frames_from_callback_.size());

  InsertVp8(seq_num + 9 , false, true , true);
  ASSERT_EQ(4UL, frames_from_callback_.size());

  InsertVp8(seq_num + 10, false, true , false);
  InsertVp8(seq_num + 11, false, false, true);
  ASSERT_EQ(5UL, frames_from_callback_.size());

  InsertVp8(seq_num + 12, true , true , true);
  ASSERT_EQ(6UL, frames_from_callback_.size());

  InsertVp8(seq_num + 13, false, true , false);
  InsertVp8(seq_num + 14, false, false, false);
  InsertVp8(seq_num + 15, false, false, false);
  InsertVp8(seq_num + 16, false, false, false);
  InsertVp8(seq_num + 17, false, false, true);
  ASSERT_EQ(7UL, frames_from_callback_.size());

  InsertVp8(seq_num + 18, false, true , true);
  ASSERT_EQ(8UL, frames_from_callback_.size());

  InsertVp8(seq_num + 19, false, true , false);
  InsertVp8(seq_num + 20, false, false, true);
  ASSERT_EQ(9UL, frames_from_callback_.size());

  InsertVp8(seq_num + 21, false, true , true);

  ASSERT_EQ(10UL, frames_from_callback_.size());
  CheckReferences(seq_num + 2);
  CheckReferences(seq_num + 4, seq_num + 2);
  CheckReferences(seq_num + 8, seq_num + 4);
  CheckReferences(seq_num + 9, seq_num + 8);
  CheckReferences(seq_num + 11, seq_num + 9);
  CheckReferences(seq_num + 12);
  CheckReferences(seq_num + 17, seq_num + 12);
  CheckReferences(seq_num + 18, seq_num + 17);
  CheckReferences(seq_num + 20, seq_num + 18);
  CheckReferences(seq_num + 21, seq_num + 20);
}

TEST_F(TestPacketBuffer, Vp8NoPictureIdReordered) {
  uint16_t seq_num = 0xfffa;

  //        seq_num     , keyf , first, last
  InsertVp8(seq_num + 1 , false, false, false);
  InsertVp8(seq_num     , true , true , false);
  InsertVp8(seq_num + 2 , false, false, true);
  InsertVp8(seq_num + 4 , false, false, true);
  InsertVp8(seq_num + 6 , false, false, false);
  InsertVp8(seq_num + 3 , false, true , false);
  InsertVp8(seq_num + 7 , false, false, false);
  InsertVp8(seq_num + 5 , false, true , false);
  InsertVp8(seq_num + 9 , false, true , true);
  InsertVp8(seq_num + 10, false, true , false);
  InsertVp8(seq_num + 8 , false, false, true);
  InsertVp8(seq_num + 13, false, true , false);
  InsertVp8(seq_num + 14, false, false, false);
  InsertVp8(seq_num + 12, true , true , true);
  InsertVp8(seq_num + 11, false, false, true);
  InsertVp8(seq_num + 16, false, false, false);
  InsertVp8(seq_num + 19, false, true , false);
  InsertVp8(seq_num + 15, false, false, false);
  InsertVp8(seq_num + 17, false, false, true);
  InsertVp8(seq_num + 20, false, false, true);
  InsertVp8(seq_num + 21, false, true , true);
  InsertVp8(seq_num + 18, false, true , true);

  ASSERT_EQ(10UL, frames_from_callback_.size());
  CheckReferences(seq_num + 2);
  CheckReferences(seq_num + 4, seq_num + 2);
  CheckReferences(seq_num + 8, seq_num + 4);
  CheckReferences(seq_num + 9, seq_num + 8);
  CheckReferences(seq_num + 11, seq_num + 9);
  CheckReferences(seq_num + 12);
  CheckReferences(seq_num + 17, seq_num + 12);
  CheckReferences(seq_num + 18, seq_num + 17);
  CheckReferences(seq_num + 20, seq_num + 18);
  CheckReferences(seq_num + 21, seq_num + 20);
}


TEST_F(TestPacketBuffer, Vp8KeyFrameReferences) {
  uint16_t pid = Rand();
  //        seq_num, keyf, first, last, sync , pid, tid, tl0
  InsertVp8(Rand() , true, true , true, false, pid, 0  , 0);

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckReferences(pid);
}

// Test with 1 temporal layer.
TEST_F(TestPacketBuffer, Vp8TemporalLayers_0) {
  uint16_t pid = Rand();
  uint16_t seq_num = Rand();

  //        seq_num    , keyf , first, last, sync , pid    , tid, tl0
  InsertVp8(seq_num    , true , true , true, false, pid    , 0  , 1);
  InsertVp8(seq_num + 1, false, true , true, false, pid + 1, 0  , 2);
  InsertVp8(seq_num + 2, false, true , true, false, pid + 2, 0  , 3);
  InsertVp8(seq_num + 3, false, true , true, false, pid + 3, 0  , 4);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckReferences(pid);
  CheckReferences(pid + 1, pid);
  CheckReferences(pid + 2, pid + 1);
  CheckReferences(pid + 3, pid + 2);
}

// Test with 1 temporal layer.
TEST_F(TestPacketBuffer, Vp8TemporalLayersReordering_0) {
  uint16_t pid = Rand();
  uint16_t seq_num = Rand();

  //        seq_num    , keyf , first, last, sync , pid    , tid, tl0
  InsertVp8(seq_num    , true , true , true, false, pid    , 0  , 1);
  InsertVp8(seq_num + 1, false, true , true, false, pid + 1, 0  , 2);
  InsertVp8(seq_num + 3, false, true , true, false, pid + 3, 0  , 4);
  InsertVp8(seq_num + 2, false, true , true, false, pid + 2, 0  , 3);
  InsertVp8(seq_num + 5, false, true , true, false, pid + 5, 0  , 6);
  InsertVp8(seq_num + 6, false, true , true, false, pid + 6, 0  , 7);
  InsertVp8(seq_num + 4, false, true , true, false, pid + 4, 0  , 5);

  ASSERT_EQ(7UL, frames_from_callback_.size());
  CheckReferences(pid);
  CheckReferences(pid + 1, pid);
  CheckReferences(pid + 2, pid + 1);
  CheckReferences(pid + 3, pid + 2);
  CheckReferences(pid + 4, pid + 3);
  CheckReferences(pid + 5, pid + 4);
  CheckReferences(pid + 6, pid + 5);
}

// Test with 2 temporal layers in a 01 pattern.
TEST_F(TestPacketBuffer, Vp8TemporalLayers_01) {
  uint16_t pid = Rand();
  uint16_t seq_num = Rand();

  //        seq_num    , keyf , first, last, sync , pid    , tid, tl0
  InsertVp8(seq_num    , true , true , true, false, pid    , 0, 255);
  InsertVp8(seq_num + 1, false, true , true, true , pid + 1, 1, 255);
  InsertVp8(seq_num + 2, false, true , true, false, pid + 2, 0, 0);
  InsertVp8(seq_num + 3, false, true , true, false, pid + 3, 1, 0);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckReferences(pid);
  CheckReferences(pid + 1, pid);
  CheckReferences(pid + 2, pid);
  CheckReferences(pid + 3, pid + 1, pid + 2);
}

// Test with 2 temporal layers in a 01 pattern.
TEST_F(TestPacketBuffer, Vp8TemporalLayersReordering_01) {
  uint16_t pid = Rand();
  uint16_t seq_num = Rand();

  //        seq_num    , keyf , first, last, sync , pid    , tid, tl0
  InsertVp8(seq_num + 1, false, true , true, true , pid + 1, 1  , 255);
  InsertVp8(seq_num    , true , true , true, false, pid    , 0  , 255);
  InsertVp8(seq_num + 3, false, true , true, false, pid + 3, 1  , 0);
  InsertVp8(seq_num + 5, false, true , true, false, pid + 5, 1  , 1);
  InsertVp8(seq_num + 2, false, true , true, false, pid + 2, 0  , 0);
  InsertVp8(seq_num + 4, false, true , true, false, pid + 4, 0  , 1);
  InsertVp8(seq_num + 6, false, true , true, false, pid + 6, 0  , 2);
  InsertVp8(seq_num + 7, false, true , true, false, pid + 7, 1  , 2);

  ASSERT_EQ(8UL, frames_from_callback_.size());
  CheckReferences(pid);
  CheckReferences(pid + 1, pid);
  CheckReferences(pid + 2, pid);
  CheckReferences(pid + 3, pid + 1, pid + 2);
  CheckReferences(pid + 4, pid + 2);
  CheckReferences(pid + 5, pid + 3, pid + 4);
  CheckReferences(pid + 6, pid + 4);
  CheckReferences(pid + 7, pid + 5, pid + 6);
}

// Test with 3 temporal layers in a 0212 pattern.
TEST_F(TestPacketBuffer, Vp8TemporalLayers_0212) {
  uint16_t pid = Rand();
  uint16_t seq_num = Rand();

  //        seq_num     , keyf , first, last, sync  , pid     , tid, tl0
  InsertVp8(seq_num     , true , true , true , false, pid     , 0  , 55);
  InsertVp8(seq_num + 1 , false, true , true , true , pid + 1 , 2  , 55);
  InsertVp8(seq_num + 2 , false, true , true , true , pid + 2 , 1  , 55);
  InsertVp8(seq_num + 3 , false, true , true , false, pid + 3 , 2  , 55);
  InsertVp8(seq_num + 4 , false, true , true , false, pid + 4 , 0  , 56);
  InsertVp8(seq_num + 5 , false, true , true , false, pid + 5 , 2  , 56);
  InsertVp8(seq_num + 6 , false, true , true , false, pid + 6 , 1  , 56);
  InsertVp8(seq_num + 7 , false, true , true , false, pid + 7 , 2  , 56);
  InsertVp8(seq_num + 8 , false, true , true , false, pid + 8 , 0  , 57);
  InsertVp8(seq_num + 9 , false, true , true , true , pid + 9 , 2  , 57);
  InsertVp8(seq_num + 10, false, true , true , true , pid + 10, 1  , 57);
  InsertVp8(seq_num + 11, false, true , true , false, pid + 11, 2  , 57);

  ASSERT_EQ(12UL, frames_from_callback_.size());
  CheckReferences(pid);
  CheckReferences(pid + 1 , pid);
  CheckReferences(pid + 2 , pid);
  CheckReferences(pid + 3 , pid, pid + 1, pid + 2);
  CheckReferences(pid + 4 , pid);
  CheckReferences(pid + 5 , pid + 2, pid + 3, pid + 4);
  CheckReferences(pid + 6 , pid + 2, pid + 4);
  CheckReferences(pid + 7 , pid + 4, pid + 5, pid + 6);
  CheckReferences(pid + 8 , pid + 4);
  CheckReferences(pid + 9 , pid + 8);
  CheckReferences(pid + 10, pid + 8);
  CheckReferences(pid + 11, pid + 8, pid + 9, pid + 10);
}

// Test with 3 temporal layers in a 0212 pattern.
TEST_F(TestPacketBuffer, Vp8TemporalLayersReordering_0212) {
  uint16_t pid = 126;
  uint16_t seq_num = Rand();

  //        seq_num     , keyf , first, last, sync , pid     , tid, tl0
  InsertVp8(seq_num + 1 , false, true , true, true , pid + 1 , 2  , 55);
  InsertVp8(seq_num     , true , true , true, false, pid     , 0  , 55);
  InsertVp8(seq_num + 2 , false, true , true, true , pid + 2 , 1  , 55);
  InsertVp8(seq_num + 4 , false, true , true, false, pid + 4 , 0  , 56);
  InsertVp8(seq_num + 5 , false, true , true, false, pid + 5 , 2  , 56);
  InsertVp8(seq_num + 3 , false, true , true, false, pid + 3 , 2  , 55);
  InsertVp8(seq_num + 7 , false, true , true, false, pid + 7 , 2  , 56);
  InsertVp8(seq_num + 9 , false, true , true, true , pid + 9 , 2  , 57);
  InsertVp8(seq_num + 6 , false, true , true, false, pid + 6 , 1  , 56);
  InsertVp8(seq_num + 8 , false, true , true, false, pid + 8 , 0  , 57);
  InsertVp8(seq_num + 11, false, true , true, false, pid + 11, 2  , 57);
  InsertVp8(seq_num + 10, false, true , true, true , pid + 10, 1  , 57);

  ASSERT_EQ(12UL, frames_from_callback_.size());
  CheckReferences(pid);
  CheckReferences(pid + 1 , pid);
  CheckReferences(pid + 2 , pid);
  CheckReferences(pid + 3 , pid, pid + 1, pid + 2);
  CheckReferences(pid + 4 , pid);
  CheckReferences(pid + 5 , pid + 2, pid + 3, pid + 4);
  CheckReferences(pid + 6 , pid + 2, pid + 4);
  CheckReferences(pid + 7 , pid + 4, pid + 5, pid + 6);
  CheckReferences(pid + 8 , pid + 4);
  CheckReferences(pid + 9 , pid + 8);
  CheckReferences(pid + 10, pid + 8);
  CheckReferences(pid + 11, pid + 8, pid + 9, pid + 10);
}

TEST_F(TestPacketBuffer, Vp8InsertManyFrames_0212) {
  uint16_t pid = Rand();
  uint16_t seq_num = Rand();

  const int keyframes_to_insert = 50;
  const int frames_per_keyframe = 120;  // Should be a multiple of 4.
  uint8_t tl0 = 128;

  for (int k = 0; k < keyframes_to_insert; ++k) {
    //        seq_num    , keyf , first, last , sync , pid    , tid, tl0
    InsertVp8(seq_num    , true , true , true , false, pid    , 0  , tl0);
    InsertVp8(seq_num + 1, false, true , true , true , pid + 1, 2  , tl0);
    InsertVp8(seq_num + 2, false, true , true , true , pid + 2, 1  , tl0);
    InsertVp8(seq_num + 3, false, true , true , false, pid + 3, 2  , tl0);
    CheckReferences(pid);
    CheckReferences(pid + 1, pid);
    CheckReferences(pid + 2, pid);
    CheckReferences(pid + 3, pid, pid + 1, pid + 2);
    frames_from_callback_.clear();
    ++tl0;

    for (int f = 4; f < frames_per_keyframe; f += 4) {
      uint16_t sf = seq_num + f;
      uint16_t pidf = pid + f;

      //        seq_num, keyf , first, last, sync , pid     , tid, tl0
      InsertVp8(sf     , false, true , true, false, pidf    , 0  , tl0);
      InsertVp8(sf + 1 , false, true , true, false, pidf + 1, 2  , tl0);
      InsertVp8(sf + 2 , false, true , true, false, pidf + 2, 1  , tl0);
      InsertVp8(sf + 3 , false, true , true, false, pidf + 3, 2  , tl0);
      CheckReferences(pidf, pidf - 4);
      CheckReferences(pidf + 1, pidf, pidf - 1, pidf - 2);
      CheckReferences(pidf + 2, pidf, pidf - 2);
      CheckReferences(pidf + 3, pidf, pidf + 1, pidf + 2);
      frames_from_callback_.clear();
      ++tl0;
    }

    pid += frames_per_keyframe;
    seq_num += frames_per_keyframe;
  }
}

TEST_F(TestPacketBuffer, Vp8LayerSync) {
  uint16_t pid = Rand();
  uint16_t seq_num = Rand();

  //        seq_num     , keyf , first, last, sync , pid     , tid, tl0
  InsertVp8(seq_num     , true , true , true, false, pid     , 0  , 0);
  InsertVp8(seq_num + 1 , false, true , true, true , pid + 1 , 1  , 0);
  InsertVp8(seq_num + 2 , false, true , true, false, pid + 2 , 0  , 1);
  ASSERT_EQ(3UL, frames_from_callback_.size());

  InsertVp8(seq_num + 4 , false, true , true, false, pid + 4 , 0  , 2);
  InsertVp8(seq_num + 5 , false, true , true, true , pid + 5 , 1  , 2);
  InsertVp8(seq_num + 6 , false, true , true, false, pid + 6 , 0  , 3);
  InsertVp8(seq_num + 7 , false, true , true, false, pid + 7 , 1  , 3);

  ASSERT_EQ(7UL, frames_from_callback_.size());
  CheckReferences(pid);
  CheckReferences(pid + 1, pid);
  CheckReferences(pid + 2, pid);
  CheckReferences(pid + 4, pid + 2);
  CheckReferences(pid + 5, pid + 4);
  CheckReferences(pid + 6, pid + 4);
  CheckReferences(pid + 7, pid + 6, pid + 5);
}

TEST_F(TestPacketBuffer, Vp8InsertLargeFrames) {
  packet_buffer_.reset(new PacketBuffer(1 << 3, 1 << 12, this));
  uint16_t pid = Rand();
  uint16_t seq_num = Rand();

  const uint16_t packets_per_frame = 1000;
  uint16_t current = seq_num;
  uint16_t end = current + packets_per_frame;

  //        seq_num  , keyf , first, last , sync , pid, tid, tl0
  InsertVp8(current++, true , true , false, false, pid, 0  , 0);
  while (current != end)
    InsertVp8(current++, false, false, false, false, pid, 0  , 0);
  InsertVp8(current++, false, false, true , false, pid, 0  , 0);
  end = current + packets_per_frame;

  for (int f = 1; f < 4; ++f) {
    InsertVp8(current++, false, true , false, false, pid + f, 0, f);
    while (current != end)
      InsertVp8(current++, false, false, false, false, pid + f, 0, f);
    InsertVp8(current++, false, false, true , false, pid + f, 0, f);
    end = current + packets_per_frame;
  }

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckReferences(pid);
  CheckReferences(pid + 1, pid);
  CheckReferences(pid + 2, pid + 1);
  CheckReferences(pid + 3, pid + 2);
}

}  // namespace video_coding
}  // namespace webrtc
