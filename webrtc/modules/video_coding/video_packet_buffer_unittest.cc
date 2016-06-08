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
#include <map>
#include <set>
#include <utility>

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
        packet_buffer_(new PacketBuffer(kStartSize, kMaxSize, this)),
        frames_from_callback_(FrameComp()) {}

  uint16_t Rand() { return rand_.Rand(std::numeric_limits<uint16_t>::max()); }

  void OnCompleteFrame(std::unique_ptr<FrameObject> frame) override {
    uint16_t pid = frame->picture_id;
    uint16_t sidx = frame->spatial_layer;
    auto frame_it = frames_from_callback_.find(std::make_pair(pid, sidx));
    if (frame_it != frames_from_callback_.end()) {
      ADD_FAILURE() << "Already received frame with (pid:sidx): ("
                    << pid << ":" << sidx << ")";
      return;
    }

    frames_from_callback_.insert(
        std::make_pair(std::make_pair(pid, sidx), std::move(frame)));
  }

  void TearDown() override {
    // All frame objects must be destroyed before the packet buffer since
    // a frame object will try to remove itself from the packet buffer
    // upon destruction.
    frames_from_callback_.clear();
  }

  // Short version of true and false.
  enum {
    kT = true,
    kF = false
  };

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
    packet.video_header.codecHeader.VP8.pictureId = pid % (1 << 15);
    packet.video_header.codecHeader.VP8.temporalIdx = tid;
    packet.video_header.codecHeader.VP8.tl0PicIdx = tl0;
    packet.video_header.codecHeader.VP8.layerSync = sync;

    EXPECT_TRUE(packet_buffer_->InsertPacket(packet));
  }

  // Insert a Vp9 packet into the packet buffer.
  void InsertVp9Gof(uint16_t seq_num,              // packet sequence number
                    bool keyframe,                 // is keyframe
                    bool first,                    // is first packet of frame
                    bool last,                     // is last packet of frame
                    bool up = false,               // frame is up-switch point
                    int32_t pid = kNoPictureId,    // picture id
                    uint8_t sid = kNoSpatialIdx,   // spatial id
                    uint8_t tid = kNoTemporalIdx,  // temporal id
                    int32_t tl0 = kNoTl0PicIdx,    // tl0 pic index
                    GofInfoVP9* ss = nullptr,      // scalability structure
                    size_t data_size = 0,          // size of data
                    uint8_t* data = nullptr) {     // data pointer
    VCMPacket packet;
    packet.codec = kVideoCodecVP9;
    packet.seqNum = seq_num;
    packet.frameType = keyframe ? kVideoFrameKey : kVideoFrameDelta;
    packet.isFirstPacket = first;
    packet.markerBit = last;
    packet.sizeBytes = data_size;
    packet.dataPtr = data;
    packet.video_header.codecHeader.VP9.flexible_mode = false;
    packet.video_header.codecHeader.VP9.picture_id = pid % (1 << 15);
    packet.video_header.codecHeader.VP9.temporal_idx = tid;
    packet.video_header.codecHeader.VP9.spatial_idx = sid;
    packet.video_header.codecHeader.VP9.tl0_pic_idx = tl0;
    packet.video_header.codecHeader.VP9.temporal_up_switch = up;
    if (ss != nullptr) {
      packet.video_header.codecHeader.VP9.ss_data_available = true;
      packet.video_header.codecHeader.VP9.gof = *ss;
    }

    EXPECT_TRUE(packet_buffer_->InsertPacket(packet));
  }

  // Insert a Vp9 packet into the packet buffer.
  void InsertVp9Flex(uint16_t seq_num,              // packet sequence number
                     bool keyframe,                 // is keyframe
                     bool first,                    // is first packet of frame
                     bool last,                     // is last packet of frame
                     bool inter,                    // depends on S-1 layer
                     int32_t pid = kNoPictureId,    // picture id
                     uint8_t sid = kNoSpatialIdx,   // spatial id
                     uint8_t tid = kNoTemporalIdx,  // temporal id
                     int32_t tl0 = kNoTl0PicIdx,    // tl0 pic index
                     std::vector<uint8_t> refs =
                       std::vector<uint8_t>(),      // frame references
                     size_t data_size = 0,          // size of data
                     uint8_t* data = nullptr) {     // data pointer
    VCMPacket packet;
    packet.codec = kVideoCodecVP9;
    packet.seqNum = seq_num;
    packet.frameType = keyframe ? kVideoFrameKey : kVideoFrameDelta;
    packet.isFirstPacket = first;
    packet.markerBit = last;
    packet.sizeBytes = data_size;
    packet.dataPtr = data;
    packet.video_header.codecHeader.VP9.inter_layer_predicted = inter;
    packet.video_header.codecHeader.VP9.flexible_mode = true;
    packet.video_header.codecHeader.VP9.picture_id = pid % (1 << 15);
    packet.video_header.codecHeader.VP9.temporal_idx = tid;
    packet.video_header.codecHeader.VP9.spatial_idx = sid;
    packet.video_header.codecHeader.VP9.tl0_pic_idx = tl0;
    packet.video_header.codecHeader.VP9.num_ref_pics = refs.size();
    for (size_t i = 0; i < refs.size(); ++i)
      packet.video_header.codecHeader.VP9.pid_diff[i] = refs[i];

    EXPECT_TRUE(packet_buffer_->InsertPacket(packet));
  }

  // Check if a frame with picture id |pid| and spatial index |sidx| has been
  // delivered from the packet buffer, and if so, if it has the references
  // specified by |refs|.
  template <typename... T>
  void CheckReferences(uint16_t pid, uint16_t sidx, T... refs) const {
    auto frame_it = frames_from_callback_.find(std::make_pair(pid, sidx));
    if (frame_it == frames_from_callback_.end()) {
      ADD_FAILURE() << "Could not find frame with (pid:sidx): ("
                    << pid << ":" << sidx << ")";
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
  void CheckReferencesGeneric(uint16_t pid, T... refs) const {
    CheckReferences(pid, 0, refs...);
  }

  template <typename... T>
  void CheckReferencesVp8(uint16_t pid, T... refs) const {
    CheckReferences(pid, 0, refs...);
  }

  template <typename... T>
  void CheckReferencesVp9(uint16_t pid, uint8_t sidx, T... refs) const {
    CheckReferences(pid, sidx, refs...);
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
  struct FrameComp {
    bool operator()(const std::pair<uint16_t, uint8_t> f1,
                    const std::pair<uint16_t, uint8_t> f2) const {
                      if (f1.first == f2.first)
                        return f1.second < f2.second;
                      return f1.first < f2.first;
                    }
  };
  std::map<std::pair<uint16_t, uint8_t>,
           std::unique_ptr<FrameObject>,
           FrameComp> frames_from_callback_;
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

TEST_F(TestPacketBuffer, NackCount) {
  uint16_t seq_num = Rand();

  VCMPacket packet;
  packet.codec = kVideoCodecGeneric;
  packet.seqNum = seq_num;
  packet.frameType = kVideoFrameKey;
  packet.isFirstPacket = true;
  packet.markerBit = false;
  packet.sizeBytes = 0;
  packet.dataPtr = nullptr;
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
  FrameObject* frame = frames_from_callback_.begin()->second.get();
  RtpFrameObject* rtp_frame = static_cast<RtpFrameObject*>(frame);
  EXPECT_EQ(3, rtp_frame->times_nacked());
}

TEST_F(TestPacketBuffer, FrameSize) {
  uint16_t seq_num = Rand();
  uint8_t data[] = {1, 2, 3, 4, 5};

  //            seq_num    , kf, frst, lst, size, data
  InsertGeneric(seq_num    , kT, kT  , kF , 5   , data);
  InsertGeneric(seq_num + 1, kT, kF  , kF , 5   , data);
  InsertGeneric(seq_num + 2, kT, kF  , kF , 5   , data);
  InsertGeneric(seq_num + 3, kT, kF  , kT , 5   , data);

  ASSERT_EQ(1UL, frames_from_callback_.size());
  EXPECT_EQ(20UL, frames_from_callback_.begin()->second->size);
}

TEST_F(TestPacketBuffer, ExpandBuffer) {
  uint16_t seq_num = Rand();

  for (int i = 0; i < kStartSize + 1; ++i) {
    //            seq_num    , kf, frst, lst
    InsertGeneric(seq_num + i, kT, kT  , kT);
  }
}

TEST_F(TestPacketBuffer, ExpandBufferOverflow) {
  uint16_t seq_num = Rand();

  for (int i = 0; i < kMaxSize; ++i) {
    //            seq_num    , kf, frst, lst
    InsertGeneric(seq_num + i, kT, kT  , kT);
  }

  VCMPacket packet;
  packet.seqNum = seq_num + kMaxSize + 1;
  packet.sizeBytes = 1;
  EXPECT_FALSE(packet_buffer_->InsertPacket(packet));
}

TEST_F(TestPacketBuffer, GenericOnePacketOneFrame) {
  //            seq_num, kf, frst, lst
  InsertGeneric(Rand() , kT, kT  , kT);
  ASSERT_EQ(1UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, GenericTwoPacketsTwoFrames) {
  uint16_t seq_num = Rand();

  //            seq_num    , kf, frst, lst
  InsertGeneric(seq_num    , kT, kT  , kT);
  InsertGeneric(seq_num + 1, kT, kT  , kT);

  EXPECT_EQ(2UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, GenericTwoPacketsOneFrames) {
  uint16_t seq_num = Rand();

  //            seq_num    , kf, frst, lst
  InsertGeneric(seq_num    , kT, kT  , kF);
  InsertGeneric(seq_num + 1, kT, kF  , kT);

  EXPECT_EQ(1UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, GenericThreePacketReorderingOneFrame) {
  uint16_t seq_num = Rand();

  //            seq_num    , kf, frst, lst
  InsertGeneric(seq_num    , kT, kT  , kF);
  InsertGeneric(seq_num + 2, kT, kF  , kT);
  InsertGeneric(seq_num + 1, kT, kF  , kF);

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
  CheckReferencesGeneric(seq_num);
  CheckReferencesGeneric(seq_num + 1, seq_num);
  CheckReferencesGeneric(seq_num + 2, seq_num + 1);
  CheckReferencesGeneric(seq_num + 3, seq_num + 2);
}

TEST_F(TestPacketBuffer, GenericFramesReordered) {
  uint16_t seq_num = Rand();

  //            seq_num    , keyf , first, last
  InsertGeneric(seq_num + 1, false, true , true);
  InsertGeneric(seq_num    , true , true , true);
  InsertGeneric(seq_num + 3, false, true , true);
  InsertGeneric(seq_num + 2, false, true , true);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckReferencesGeneric(seq_num);
  CheckReferencesGeneric(seq_num + 1, seq_num);
  CheckReferencesGeneric(seq_num + 2, seq_num + 1);
  CheckReferencesGeneric(seq_num + 3, seq_num + 2);
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

  //            seq_num    , kf, frst, lst, data_size        , data
  InsertGeneric(seq_num    , kT, kT  , kF , sizeof(many)     , many);
  InsertGeneric(seq_num + 1, kF, kF  , kF , sizeof(bitstream), bitstream);
  InsertGeneric(seq_num + 2, kF, kF  , kF , sizeof(such)     , such);
  InsertGeneric(seq_num + 3, kF, kF  , kT , sizeof(data)     , data);

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckReferencesVp8(seq_num + 3);
  EXPECT_TRUE(frames_from_callback_[std::make_pair(seq_num + 3, 0)]->
                                                          GetBitstream(result));
  EXPECT_EQ(std::strcmp("many bitstream, such data",
            reinterpret_cast<char*>(result)),
            0);
}

TEST_F(TestPacketBuffer, FreeSlotsOnFrameDestruction) {
  uint16_t seq_num = Rand();

  //            seq_num    , kf, frst, lst
  InsertGeneric(seq_num    , kT, kT  , kF);
  InsertGeneric(seq_num + 1, kF, kF  , kF);
  InsertGeneric(seq_num + 2, kF, kF  , kT);
  EXPECT_EQ(1UL, frames_from_callback_.size());

  frames_from_callback_.clear();

  //            seq_num    , kf, frst, lst
  InsertGeneric(seq_num    , kT, kT  , kF);
  InsertGeneric(seq_num + 1, kF, kF  , kF);
  InsertGeneric(seq_num + 2, kF, kF  , kT);
  EXPECT_EQ(1UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, Clear) {
  uint16_t seq_num = Rand();

  //            seq_num    , kf, frst, lst
  InsertGeneric(seq_num    , kT, kT  , kF);
  InsertGeneric(seq_num + 1, kF, kF  , kF);
  InsertGeneric(seq_num + 2, kF, kF  , kT);
  EXPECT_EQ(1UL, frames_from_callback_.size());

  packet_buffer_->Clear();

  //            seq_num                 , kf, frst, lst
  InsertGeneric(seq_num + kStartSize    , kT, kT  , kF);
  InsertGeneric(seq_num + kStartSize + 1, kF, kF  , kF);
  InsertGeneric(seq_num + kStartSize + 2, kF, kF  , kT);
  EXPECT_EQ(2UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, InvalidateFrameByClearing) {
  VCMPacket packet;
  packet.codec = kVideoCodecGeneric;
  packet.frameType = kVideoFrameKey;
  packet.isFirstPacket = kT;
  packet.markerBit = kT;
  packet.seqNum = Rand();
  EXPECT_TRUE(packet_buffer_->InsertPacket(packet));
  ASSERT_EQ(1UL, frames_from_callback_.size());

  packet_buffer_->Clear();
  EXPECT_FALSE(frames_from_callback_.begin()->second->GetBitstream(nullptr));
}

TEST_F(TestPacketBuffer, Vp8NoPictureId) {
  uint16_t seq_num = Rand();

  //        seq_num     , kf, frst, lst
  InsertVp8(seq_num     , kT, kT  , kF);
  InsertVp8(seq_num + 1 , kF, kF  , kF);
  InsertVp8(seq_num + 2 , kF, kF  , kT);
  ASSERT_EQ(1UL, frames_from_callback_.size());

  InsertVp8(seq_num + 3 , kF, kT  , kF);
  InsertVp8(seq_num + 4 , kF, kF  , kT);
  ASSERT_EQ(2UL, frames_from_callback_.size());

  InsertVp8(seq_num + 5 , kF, kT  , kF);
  InsertVp8(seq_num + 6 , kF, kF  , kF);
  InsertVp8(seq_num + 7 , kF, kF  , kF);
  InsertVp8(seq_num + 8 , kF, kF  , kT);
  ASSERT_EQ(3UL, frames_from_callback_.size());

  InsertVp8(seq_num + 9 , kF, kT  , kT);
  ASSERT_EQ(4UL, frames_from_callback_.size());

  InsertVp8(seq_num + 10, kF, kT  , kF);
  InsertVp8(seq_num + 11, kF, kF  , kT);
  ASSERT_EQ(5UL, frames_from_callback_.size());

  InsertVp8(seq_num + 12, kT, kT  , kT);
  ASSERT_EQ(6UL, frames_from_callback_.size());

  InsertVp8(seq_num + 13, kF, kT  , kF);
  InsertVp8(seq_num + 14, kF, kF  , kF);
  InsertVp8(seq_num + 15, kF, kF  , kF);
  InsertVp8(seq_num + 16, kF, kF  , kF);
  InsertVp8(seq_num + 17, kF, kF  , kT);
  ASSERT_EQ(7UL, frames_from_callback_.size());

  InsertVp8(seq_num + 18, kF, kT  , kT);
  ASSERT_EQ(8UL, frames_from_callback_.size());

  InsertVp8(seq_num + 19, kF, kT  , kF);
  InsertVp8(seq_num + 20, kF, kF  , kT);
  ASSERT_EQ(9UL, frames_from_callback_.size());

  InsertVp8(seq_num + 21, kF, kT  , kT);

  ASSERT_EQ(10UL, frames_from_callback_.size());
  CheckReferencesVp8(seq_num + 2);
  CheckReferencesVp8(seq_num + 4, seq_num + 2);
  CheckReferencesVp8(seq_num + 8, seq_num + 4);
  CheckReferencesVp8(seq_num + 9, seq_num + 8);
  CheckReferencesVp8(seq_num + 11, seq_num + 9);
  CheckReferencesVp8(seq_num + 12);
  CheckReferencesVp8(seq_num + 17, seq_num + 12);
  CheckReferencesVp8(seq_num + 18, seq_num + 17);
  CheckReferencesVp8(seq_num + 20, seq_num + 18);
  CheckReferencesVp8(seq_num + 21, seq_num + 20);
}

TEST_F(TestPacketBuffer, Vp8NoPictureIdReordered) {
  uint16_t seq_num = 0xfffa;

  //        seq_num     , kf, frst, lst
  InsertVp8(seq_num + 1 , kF, kF  , kF);
  InsertVp8(seq_num     , kT, kT  , kF);
  InsertVp8(seq_num + 2 , kF, kF  , kT);
  InsertVp8(seq_num + 4 , kF, kF  , kT);
  InsertVp8(seq_num + 6 , kF, kF  , kF);
  InsertVp8(seq_num + 3 , kF, kT  , kF);
  InsertVp8(seq_num + 7 , kF, kF  , kF);
  InsertVp8(seq_num + 5 , kF, kT  , kF);
  InsertVp8(seq_num + 9 , kF, kT  , kT);
  InsertVp8(seq_num + 10, kF, kT  , kF);
  InsertVp8(seq_num + 8 , kF, kF  , kT);
  InsertVp8(seq_num + 13, kF, kT  , kF);
  InsertVp8(seq_num + 14, kF, kF  , kF);
  InsertVp8(seq_num + 12, kT, kT  , kT);
  InsertVp8(seq_num + 11, kF, kF  , kT);
  InsertVp8(seq_num + 16, kF, kF  , kF);
  InsertVp8(seq_num + 19, kF, kT  , kF);
  InsertVp8(seq_num + 15, kF, kF  , kF);
  InsertVp8(seq_num + 17, kF, kF  , kT);
  InsertVp8(seq_num + 20, kF, kF  , kT);
  InsertVp8(seq_num + 21, kF, kT  , kT);
  InsertVp8(seq_num + 18, kF, kT  , kT);

  ASSERT_EQ(10UL, frames_from_callback_.size());
  CheckReferencesVp8(seq_num + 2);
  CheckReferencesVp8(seq_num + 4, seq_num + 2);
  CheckReferencesVp8(seq_num + 8, seq_num + 4);
  CheckReferencesVp8(seq_num + 9, seq_num + 8);
  CheckReferencesVp8(seq_num + 11, seq_num + 9);
  CheckReferencesVp8(seq_num + 12);
  CheckReferencesVp8(seq_num + 17, seq_num + 12);
  CheckReferencesVp8(seq_num + 18, seq_num + 17);
  CheckReferencesVp8(seq_num + 20, seq_num + 18);
  CheckReferencesVp8(seq_num + 21, seq_num + 20);
}


TEST_F(TestPacketBuffer, Vp8KeyFrameReferences) {
  uint16_t pid = Rand();
  //        seq_num, kf, frst, lst, sync, pid, tid, tl0
  InsertVp8(Rand() , kT, kT  , kT , kF  , pid, 0  , 0);

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
}

// Test with 1 temporal layer.
TEST_F(TestPacketBuffer, Vp8TemporalLayers_0) {
  uint16_t pid = Rand();
  uint16_t seq_num = Rand();

  //        seq_num    , kf, frst, lst, sync, pid    , tid, tl0
  InsertVp8(seq_num    , kT, kT  , kT , kF  , pid    , 0  , 1);
  InsertVp8(seq_num + 1, kF, kT  , kT , kF  , pid + 1, 0  , 2);
  InsertVp8(seq_num + 2, kF, kT  , kT , kF  , pid + 2, 0  , 3);
  InsertVp8(seq_num + 3, kF, kT  , kT , kF  , pid + 3, 0  , 4);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid + 1);
  CheckReferencesVp8(pid + 3, pid + 2);
}

// Test with 1 temporal layer.
TEST_F(TestPacketBuffer, Vp8TemporalLayersReordering_0) {
  uint16_t pid = Rand();
  uint16_t seq_num = Rand();

  //        seq_num    , kf, frst, lst, sync, pid    , tid, tl0
  InsertVp8(seq_num    , kT, kT  , kT , kF  , pid    , 0  , 1);
  InsertVp8(seq_num + 1, kF, kT  , kT , kF  , pid + 1, 0  , 2);
  InsertVp8(seq_num + 3, kF, kT  , kT , kF  , pid + 3, 0  , 4);
  InsertVp8(seq_num + 2, kF, kT  , kT , kF  , pid + 2, 0  , 3);
  InsertVp8(seq_num + 5, kF, kT  , kT , kF  , pid + 5, 0  , 6);
  InsertVp8(seq_num + 6, kF, kT  , kT , kF  , pid + 6, 0  , 7);
  InsertVp8(seq_num + 4, kF, kT  , kT , kF  , pid + 4, 0  , 5);

  ASSERT_EQ(7UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid + 1);
  CheckReferencesVp8(pid + 3, pid + 2);
  CheckReferencesVp8(pid + 4, pid + 3);
  CheckReferencesVp8(pid + 5, pid + 4);
  CheckReferencesVp8(pid + 6, pid + 5);
}

// Test with 2 temporal layers in a 01 pattern.
TEST_F(TestPacketBuffer, Vp8TemporalLayers_01) {
  uint16_t pid = Rand();
  uint16_t seq_num = Rand();

  //        seq_num    , kf, frst, lst, sync, pid    , tid, tl0
  InsertVp8(seq_num    , kT, kT  , kT , kF  , pid    , 0, 255);
  InsertVp8(seq_num + 1, kF, kT  , kT , kT  , pid + 1, 1, 255);
  InsertVp8(seq_num + 2, kF, kT  , kT , kF  , pid + 2, 0, 0);
  InsertVp8(seq_num + 3, kF, kT  , kT , kF  , pid + 3, 1, 0);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid);
  CheckReferencesVp8(pid + 3, pid + 1, pid + 2);
}

// Test with 2 temporal layers in a 01 pattern.
TEST_F(TestPacketBuffer, Vp8TemporalLayersReordering_01) {
  uint16_t pid = Rand();
  uint16_t seq_num = Rand();

  //        seq_num    , kf, frst, lst, sync, pid    , tid, tl0
  InsertVp8(seq_num + 1, kF, kT  , kT , kT  , pid + 1, 1  , 255);
  InsertVp8(seq_num    , kT, kT  , kT , kF  , pid    , 0  , 255);
  InsertVp8(seq_num + 3, kF, kT  , kT , kF  , pid + 3, 1  , 0);
  InsertVp8(seq_num + 5, kF, kT  , kT , kF  , pid + 5, 1  , 1);
  InsertVp8(seq_num + 2, kF, kT  , kT , kF  , pid + 2, 0  , 0);
  InsertVp8(seq_num + 4, kF, kT  , kT , kF  , pid + 4, 0  , 1);
  InsertVp8(seq_num + 6, kF, kT  , kT , kF  , pid + 6, 0  , 2);
  InsertVp8(seq_num + 7, kF, kT  , kT , kF  , pid + 7, 1  , 2);

  ASSERT_EQ(8UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid);
  CheckReferencesVp8(pid + 3, pid + 1, pid + 2);
  CheckReferencesVp8(pid + 4, pid + 2);
  CheckReferencesVp8(pid + 5, pid + 3, pid + 4);
  CheckReferencesVp8(pid + 6, pid + 4);
  CheckReferencesVp8(pid + 7, pid + 5, pid + 6);
}

// Test with 3 temporal layers in a 0212 pattern.
TEST_F(TestPacketBuffer, Vp8TemporalLayers_0212) {
  uint16_t pid = Rand();
  uint16_t seq_num = Rand();

  //        seq_num     , kf, frst, lst, sync, pid     , tid, tl0
  InsertVp8(seq_num     , kT, kT  , kT , kF  , pid     , 0  , 55);
  InsertVp8(seq_num + 1 , kF, kT  , kT , kT  , pid + 1 , 2  , 55);
  InsertVp8(seq_num + 2 , kF, kT  , kT , kT  , pid + 2 , 1  , 55);
  InsertVp8(seq_num + 3 , kF, kT  , kT , kF  , pid + 3 , 2  , 55);
  InsertVp8(seq_num + 4 , kF, kT  , kT , kF  , pid + 4 , 0  , 56);
  InsertVp8(seq_num + 5 , kF, kT  , kT , kF  , pid + 5 , 2  , 56);
  InsertVp8(seq_num + 6 , kF, kT  , kT , kF  , pid + 6 , 1  , 56);
  InsertVp8(seq_num + 7 , kF, kT  , kT , kF  , pid + 7 , 2  , 56);
  InsertVp8(seq_num + 8 , kF, kT  , kT , kF  , pid + 8 , 0  , 57);
  InsertVp8(seq_num + 9 , kF, kT  , kT , kT  , pid + 9 , 2  , 57);
  InsertVp8(seq_num + 10, kF, kT  , kT , kT  , pid + 10, 1  , 57);
  InsertVp8(seq_num + 11, kF, kT  , kT , kF  , pid + 11, 2  , 57);

  ASSERT_EQ(12UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1 , pid);
  CheckReferencesVp8(pid + 2 , pid);
  CheckReferencesVp8(pid + 3 , pid, pid + 1, pid + 2);
  CheckReferencesVp8(pid + 4 , pid);
  CheckReferencesVp8(pid + 5 , pid + 2, pid + 3, pid + 4);
  CheckReferencesVp8(pid + 6 , pid + 2, pid + 4);
  CheckReferencesVp8(pid + 7 , pid + 4, pid + 5, pid + 6);
  CheckReferencesVp8(pid + 8 , pid + 4);
  CheckReferencesVp8(pid + 9 , pid + 8);
  CheckReferencesVp8(pid + 10, pid + 8);
  CheckReferencesVp8(pid + 11, pid + 8, pid + 9, pid + 10);
}

// Test with 3 temporal layers in a 0212 pattern.
TEST_F(TestPacketBuffer, Vp8TemporalLayersReordering_0212) {
  uint16_t pid = 126;
  uint16_t seq_num = Rand();

  //        seq_num     , kf, frst, lst, sync, pid     , tid, tl0
  InsertVp8(seq_num + 1 , kF, kT  , kT , kT  , pid + 1 , 2  , 55);
  InsertVp8(seq_num     , kT, kT  , kT , kF  , pid     , 0  , 55);
  InsertVp8(seq_num + 2 , kF, kT  , kT , kT  , pid + 2 , 1  , 55);
  InsertVp8(seq_num + 4 , kF, kT  , kT , kF  , pid + 4 , 0  , 56);
  InsertVp8(seq_num + 5 , kF, kT  , kT , kF  , pid + 5 , 2  , 56);
  InsertVp8(seq_num + 3 , kF, kT  , kT , kF  , pid + 3 , 2  , 55);
  InsertVp8(seq_num + 7 , kF, kT  , kT , kF  , pid + 7 , 2  , 56);
  InsertVp8(seq_num + 9 , kF, kT  , kT , kT  , pid + 9 , 2  , 57);
  InsertVp8(seq_num + 6 , kF, kT  , kT , kF  , pid + 6 , 1  , 56);
  InsertVp8(seq_num + 8 , kF, kT  , kT , kF  , pid + 8 , 0  , 57);
  InsertVp8(seq_num + 11, kF, kT  , kT , kF  , pid + 11, 2  , 57);
  InsertVp8(seq_num + 10, kF, kT  , kT , kT  , pid + 10, 1  , 57);

  ASSERT_EQ(12UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1 , pid);
  CheckReferencesVp8(pid + 2 , pid);
  CheckReferencesVp8(pid + 3 , pid, pid + 1, pid + 2);
  CheckReferencesVp8(pid + 4 , pid);
  CheckReferencesVp8(pid + 5 , pid + 2, pid + 3, pid + 4);
  CheckReferencesVp8(pid + 6 , pid + 2, pid + 4);
  CheckReferencesVp8(pid + 7 , pid + 4, pid + 5, pid + 6);
  CheckReferencesVp8(pid + 8 , pid + 4);
  CheckReferencesVp8(pid + 9 , pid + 8);
  CheckReferencesVp8(pid + 10, pid + 8);
  CheckReferencesVp8(pid + 11, pid + 8, pid + 9, pid + 10);
}

TEST_F(TestPacketBuffer, Vp8InsertManyFrames_0212) {
  uint16_t pid = Rand();
  uint16_t seq_num = Rand();

  const int keyframes_to_insert = 50;
  const int frames_per_keyframe = 120;  // Should be a multiple of 4.
  uint8_t tl0 = 128;

  for (int k = 0; k < keyframes_to_insert; ++k) {
    //        seq_num    , keyf, frst, lst, sync, pid    , tid, tl0
    InsertVp8(seq_num    , kT  , kT  , kT , kF  , pid    , 0  , tl0);
    InsertVp8(seq_num + 1, kF  , kT  , kT , kT  , pid + 1, 2  , tl0);
    InsertVp8(seq_num + 2, kF  , kT  , kT , kT  , pid + 2, 1  , tl0);
    InsertVp8(seq_num + 3, kF  , kT  , kT , kF  , pid + 3, 2  , tl0);
    CheckReferencesVp8(pid);
    CheckReferencesVp8(pid + 1, pid);
    CheckReferencesVp8(pid + 2, pid);
    CheckReferencesVp8(pid + 3, pid, pid + 1, pid + 2);
    frames_from_callback_.clear();
    ++tl0;

    for (int f = 4; f < frames_per_keyframe; f += 4) {
      uint16_t sf = seq_num + f;
      uint16_t pidf = pid + f;

      //        seq_num, keyf, frst, lst, sync, pid     , tid, tl0
      InsertVp8(sf     , kF  , kT  , kT , kF  , pidf    , 0  , tl0);
      InsertVp8(sf + 1 , kF  , kT  , kT , kF  , pidf + 1, 2  , tl0);
      InsertVp8(sf + 2 , kF  , kT  , kT , kF  , pidf + 2, 1  , tl0);
      InsertVp8(sf + 3 , kF  , kT  , kT , kF  , pidf + 3, 2  , tl0);
      CheckReferencesVp8(pidf, pidf - 4);
      CheckReferencesVp8(pidf + 1, pidf, pidf - 1, pidf - 2);
      CheckReferencesVp8(pidf + 2, pidf, pidf - 2);
      CheckReferencesVp8(pidf + 3, pidf, pidf + 1, pidf + 2);
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

  //        seq_num     , keyf, frst, lst, sync, pid     , tid, tl0
  InsertVp8(seq_num     , kT  , kT  , kT , kF  , pid     , 0  , 0);
  InsertVp8(seq_num + 1 , kF  , kT  , kT , kT  , pid + 1 , 1  , 0);
  InsertVp8(seq_num + 2 , kF  , kT  , kT , kF  , pid + 2 , 0  , 1);
  ASSERT_EQ(3UL, frames_from_callback_.size());

  InsertVp8(seq_num + 4 , kF  , kT  , kT , kF  , pid + 4 , 0  , 2);
  InsertVp8(seq_num + 5 , kF  , kT  , kT , kT  , pid + 5 , 1  , 2);
  InsertVp8(seq_num + 6 , kF  , kT  , kT , kF  , pid + 6 , 0  , 3);
  InsertVp8(seq_num + 7 , kF  , kT  , kT , kF  , pid + 7 , 1  , 3);

  ASSERT_EQ(7UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid);
  CheckReferencesVp8(pid + 4, pid + 2);
  CheckReferencesVp8(pid + 5, pid + 4);
  CheckReferencesVp8(pid + 6, pid + 4);
  CheckReferencesVp8(pid + 7, pid + 6, pid + 5);
}

TEST_F(TestPacketBuffer, Vp8InsertLargeFrames) {
  packet_buffer_.reset(new PacketBuffer(1 << 3, 1 << 12, this));
  uint16_t pid = Rand();
  uint16_t seq_num = Rand();

  const uint16_t packets_per_frame = 1000;
  uint16_t current = seq_num;
  uint16_t end = current + packets_per_frame;

  //        seq_num  , keyf, frst, lst, sync, pid, tid, tl0
  InsertVp8(current++, kT  , kT  , kF , kF  , pid, 0  , 0);
  while (current != end)
    InsertVp8(current++, kF  , kF  , kF , kF  , pid, 0  , 0);
  InsertVp8(current++, kF  , kF  , kT , kF  , pid, 0  , 0);
  end = current + packets_per_frame;

  for (int f = 1; f < 4; ++f) {
    InsertVp8(current++, kF  , kT  , kF , kF  , pid + f, 0, f);
    while (current != end)
      InsertVp8(current++, kF  , kF  , kF , kF  , pid + f, 0, f);
    InsertVp8(current++, kF  , kF  , kT , kF  , pid + f, 0, f);
    end = current + packets_per_frame;
  }

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid + 1);
  CheckReferencesVp8(pid + 3, pid + 2);
}

TEST_F(TestPacketBuffer, Vp9GofInsertOneFrame) {
  uint16_t pid = Rand();
  uint16_t seq_num = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode1);

  //           seq_num, keyf, frst, lst, up, pid, sid, tid, tl0, ss
  InsertVp9Gof(seq_num, kT  , kT  , kT , kF, pid, 0  , 0  , 0  , &ss);

  CheckReferencesVp9(pid, 0);
}

TEST_F(TestPacketBuffer, Vp9NoPictureIdReordered) {
  uint16_t sn = 0xfffa;

  //           sn     , kf, frst, lst
  InsertVp9Gof(sn + 1 , kF, kF  , kF);
  InsertVp9Gof(sn     , kT, kT  , kF);
  InsertVp9Gof(sn + 2 , kF, kF  , kT);
  InsertVp9Gof(sn + 4 , kF, kF  , kT);
  InsertVp9Gof(sn + 6 , kF, kF  , kF);
  InsertVp9Gof(sn + 3 , kF, kT  , kF);
  InsertVp9Gof(sn + 7 , kF, kF  , kF);
  InsertVp9Gof(sn + 5 , kF, kT  , kF);
  InsertVp9Gof(sn + 9 , kF, kT  , kT);
  InsertVp9Gof(sn + 10, kF, kT  , kF);
  InsertVp9Gof(sn + 8 , kF, kF  , kT);
  InsertVp9Gof(sn + 13, kF, kT  , kF);
  InsertVp9Gof(sn + 14, kF, kF  , kF);
  InsertVp9Gof(sn + 12, kT, kT  , kT);
  InsertVp9Gof(sn + 11, kF, kF  , kT);
  InsertVp9Gof(sn + 16, kF, kF  , kF);
  InsertVp9Gof(sn + 19, kF, kT  , kF);
  InsertVp9Gof(sn + 15, kF, kF  , kF);
  InsertVp9Gof(sn + 17, kF, kF  , kT);
  InsertVp9Gof(sn + 20, kF, kF  , kT);
  InsertVp9Gof(sn + 21, kF, kT  , kT);
  InsertVp9Gof(sn + 18, kF, kT  , kT);

  ASSERT_EQ(10UL, frames_from_callback_.size());
  CheckReferencesVp9(sn + 2 , 0);
  CheckReferencesVp9(sn + 4 , 0, sn + 2);
  CheckReferencesVp9(sn + 8 , 0, sn + 4);
  CheckReferencesVp9(sn + 9 , 0, sn + 8);
  CheckReferencesVp9(sn + 11, 0, sn + 9);
  CheckReferencesVp9(sn + 12, 0);
  CheckReferencesVp9(sn + 17, 0, sn + 12);
  CheckReferencesVp9(sn + 18, 0, sn + 17);
  CheckReferencesVp9(sn + 20, 0, sn + 18);
  CheckReferencesVp9(sn + 21, 0, sn + 20);
}

TEST_F(TestPacketBuffer, Vp9GofTemporalLayers_0) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode1);  // Only 1 spatial layer.

  //           sn     , kf, frst, lst, up, pid     , sid, tid, tl0, ss
  InsertVp9Gof(sn     , kT, kT  , kT , kF, pid     , 0  , 0  , 0  , &ss);
  InsertVp9Gof(sn + 1 , kF, kT  , kT , kF, pid + 1 , 0  , 0  , 1);
  InsertVp9Gof(sn + 2 , kF, kT  , kT , kF, pid + 2 , 0  , 0  , 2);
  InsertVp9Gof(sn + 3 , kF, kT  , kT , kF, pid + 3 , 0  , 0  , 3);
  InsertVp9Gof(sn + 4 , kF, kT  , kT , kF, pid + 4 , 0  , 0  , 4);
  InsertVp9Gof(sn + 5 , kF, kT  , kT , kF, pid + 5 , 0  , 0  , 5);
  InsertVp9Gof(sn + 6 , kF, kT  , kT , kF, pid + 6 , 0  , 0  , 6);
  InsertVp9Gof(sn + 7 , kF, kT  , kT , kF, pid + 7 , 0  , 0  , 7);
  InsertVp9Gof(sn + 8 , kF, kT  , kT , kF, pid + 8 , 0  , 0  , 8);
  InsertVp9Gof(sn + 9 , kF, kT  , kT , kF, pid + 9 , 0  , 0  , 9);
  InsertVp9Gof(sn + 10, kF, kT  , kT , kF, pid + 10, 0  , 0  , 10);
  InsertVp9Gof(sn + 11, kF, kT  , kT , kF, pid + 11, 0  , 0  , 11);
  InsertVp9Gof(sn + 12, kF, kT  , kT , kF, pid + 12, 0  , 0  , 12);
  InsertVp9Gof(sn + 13, kF, kT  , kT , kF, pid + 13, 0  , 0  , 13);
  InsertVp9Gof(sn + 14, kF, kT  , kT , kF, pid + 14, 0  , 0  , 14);
  InsertVp9Gof(sn + 15, kF, kT  , kT , kF, pid + 15, 0  , 0  , 15);
  InsertVp9Gof(sn + 16, kF, kT  , kT , kF, pid + 16, 0  , 0  , 16);
  InsertVp9Gof(sn + 17, kF, kT  , kT , kF, pid + 17, 0  , 0  , 17);
  InsertVp9Gof(sn + 18, kF, kT  , kT , kF, pid + 18, 0  , 0  , 18);
  InsertVp9Gof(sn + 19, kF, kT  , kT , kF, pid + 19, 0  , 0  , 19);

  ASSERT_EQ(20UL, frames_from_callback_.size());
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid + 1 , 0, pid);
  CheckReferencesVp9(pid + 2 , 0, pid + 1);
  CheckReferencesVp9(pid + 3 , 0, pid + 2);
  CheckReferencesVp9(pid + 4 , 0, pid + 3);
  CheckReferencesVp9(pid + 5 , 0, pid + 4);
  CheckReferencesVp9(pid + 6 , 0, pid + 5);
  CheckReferencesVp9(pid + 7 , 0, pid + 6);
  CheckReferencesVp9(pid + 8 , 0, pid + 7);
  CheckReferencesVp9(pid + 9 , 0, pid + 8);
  CheckReferencesVp9(pid + 10, 0, pid + 9);
  CheckReferencesVp9(pid + 11, 0, pid + 10);
  CheckReferencesVp9(pid + 12, 0, pid + 11);
  CheckReferencesVp9(pid + 13, 0, pid + 12);
  CheckReferencesVp9(pid + 14, 0, pid + 13);
  CheckReferencesVp9(pid + 15, 0, pid + 14);
  CheckReferencesVp9(pid + 16, 0, pid + 15);
  CheckReferencesVp9(pid + 17, 0, pid + 16);
  CheckReferencesVp9(pid + 18, 0, pid + 17);
  CheckReferencesVp9(pid + 19, 0, pid + 18);
}

TEST_F(TestPacketBuffer, Vp9GofTemporalLayersReordered_0) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode1);  // Only 1 spatial layer.

  //           sn     , kf, frst, lst, up, pid     , sid, tid, tl0, ss
  InsertVp9Gof(sn + 2 , kF, kT  , kT , kF, pid + 2 , 0  , 0  , 2);
  InsertVp9Gof(sn + 1 , kF, kT  , kT , kF, pid + 1 , 0  , 0  , 1);
  InsertVp9Gof(sn     , kT, kT  , kT , kF, pid     , 0  , 0  , 0  , &ss);
  InsertVp9Gof(sn + 4 , kF, kT  , kT , kF, pid + 4 , 0  , 0  , 4);
  InsertVp9Gof(sn + 3 , kF, kT  , kT , kF, pid + 3 , 0  , 0  , 3);
  InsertVp9Gof(sn + 5 , kF, kT  , kT , kF, pid + 5 , 0  , 0  , 5);
  InsertVp9Gof(sn + 7 , kF, kT  , kT , kF, pid + 7 , 0  , 0  , 7);
  InsertVp9Gof(sn + 6 , kF, kT  , kT , kF, pid + 6 , 0  , 0  , 6);
  InsertVp9Gof(sn + 8 , kF, kT  , kT , kF, pid + 8 , 0  , 0  , 8);
  InsertVp9Gof(sn + 10, kF, kT  , kT , kF, pid + 10, 0  , 0  , 10);
  InsertVp9Gof(sn + 13, kF, kT  , kT , kF, pid + 13, 0  , 0  , 13);
  InsertVp9Gof(sn + 11, kF, kT  , kT , kF, pid + 11, 0  , 0  , 11);
  InsertVp9Gof(sn + 9 , kF, kT  , kT , kF, pid + 9 , 0  , 0  , 9);
  InsertVp9Gof(sn + 16, kF, kT  , kT , kF, pid + 16, 0  , 0  , 16);
  InsertVp9Gof(sn + 14, kF, kT  , kT , kF, pid + 14, 0  , 0  , 14);
  InsertVp9Gof(sn + 15, kF, kT  , kT , kF, pid + 15, 0  , 0  , 15);
  InsertVp9Gof(sn + 12, kF, kT  , kT , kF, pid + 12, 0  , 0  , 12);
  InsertVp9Gof(sn + 17, kF, kT  , kT , kF, pid + 17, 0  , 0  , 17);
  InsertVp9Gof(sn + 19, kF, kT  , kT , kF, pid + 19, 0  , 0  , 19);
  InsertVp9Gof(sn + 18, kF, kT  , kT , kF, pid + 18, 0  , 0  , 18);

  ASSERT_EQ(20UL, frames_from_callback_.size());
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid + 1 , 0, pid);
  CheckReferencesVp9(pid + 2 , 0, pid + 1);
  CheckReferencesVp9(pid + 3 , 0, pid + 2);
  CheckReferencesVp9(pid + 4 , 0, pid + 3);
  CheckReferencesVp9(pid + 5 , 0, pid + 4);
  CheckReferencesVp9(pid + 6 , 0, pid + 5);
  CheckReferencesVp9(pid + 7 , 0, pid + 6);
  CheckReferencesVp9(pid + 8 , 0, pid + 7);
  CheckReferencesVp9(pid + 9 , 0, pid + 8);
  CheckReferencesVp9(pid + 10, 0, pid + 9);
  CheckReferencesVp9(pid + 11, 0, pid + 10);
  CheckReferencesVp9(pid + 12, 0, pid + 11);
  CheckReferencesVp9(pid + 13, 0, pid + 12);
  CheckReferencesVp9(pid + 14, 0, pid + 13);
  CheckReferencesVp9(pid + 15, 0, pid + 14);
  CheckReferencesVp9(pid + 16, 0, pid + 15);
  CheckReferencesVp9(pid + 17, 0, pid + 16);
  CheckReferencesVp9(pid + 18, 0, pid + 17);
  CheckReferencesVp9(pid + 19, 0, pid + 18);
}

TEST_F(TestPacketBuffer, Vp9GofTemporalLayers_01) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode2);  // 0101 pattern

  //           sn     , kf, frst, lst, up, pid     , sid, tid, tl0, ss
  InsertVp9Gof(sn     , kT, kT  , kT , kF, pid     , 0  , 0  , 0  , &ss);
  InsertVp9Gof(sn + 1 , kF, kT  , kT , kF, pid + 1 , 0  , 1  , 0);
  InsertVp9Gof(sn + 2 , kF, kT  , kT , kF, pid + 2 , 0  , 0  , 1);
  InsertVp9Gof(sn + 3 , kF, kT  , kT , kF, pid + 3 , 0  , 1  , 1);
  InsertVp9Gof(sn + 4 , kF, kT  , kT , kF, pid + 4 , 0  , 0  , 2);
  InsertVp9Gof(sn + 5 , kF, kT  , kT , kF, pid + 5 , 0  , 1  , 2);
  InsertVp9Gof(sn + 6 , kF, kT  , kT , kF, pid + 6 , 0  , 0  , 3);
  InsertVp9Gof(sn + 7 , kF, kT  , kT , kF, pid + 7 , 0  , 1  , 3);
  InsertVp9Gof(sn + 8 , kF, kT  , kT , kF, pid + 8 , 0  , 0  , 4);
  InsertVp9Gof(sn + 9 , kF, kT  , kT , kF, pid + 9 , 0  , 1  , 4);
  InsertVp9Gof(sn + 10, kF, kT  , kT , kF, pid + 10, 0  , 0  , 5);
  InsertVp9Gof(sn + 11, kF, kT  , kT , kF, pid + 11, 0  , 1  , 5);
  InsertVp9Gof(sn + 12, kF, kT  , kT , kF, pid + 12, 0  , 0  , 6);
  InsertVp9Gof(sn + 13, kF, kT  , kT , kF, pid + 13, 0  , 1  , 6);
  InsertVp9Gof(sn + 14, kF, kT  , kT , kF, pid + 14, 0  , 0  , 7);
  InsertVp9Gof(sn + 15, kF, kT  , kT , kF, pid + 15, 0  , 1  , 7);
  InsertVp9Gof(sn + 16, kF, kT  , kT , kF, pid + 16, 0  , 0  , 8);
  InsertVp9Gof(sn + 17, kF, kT  , kT , kF, pid + 17, 0  , 1  , 8);
  InsertVp9Gof(sn + 18, kF, kT  , kT , kF, pid + 18, 0  , 0  , 9);
  InsertVp9Gof(sn + 19, kF, kT  , kT , kF, pid + 19, 0  , 1  , 9);

  ASSERT_EQ(20UL, frames_from_callback_.size());
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid + 1 , 0, pid);
  CheckReferencesVp9(pid + 2 , 0, pid);
  CheckReferencesVp9(pid + 3 , 0, pid + 2);
  CheckReferencesVp9(pid + 4 , 0, pid + 2);
  CheckReferencesVp9(pid + 5 , 0, pid + 4);
  CheckReferencesVp9(pid + 6 , 0, pid + 4);
  CheckReferencesVp9(pid + 7 , 0, pid + 6);
  CheckReferencesVp9(pid + 8 , 0, pid + 6);
  CheckReferencesVp9(pid + 9 , 0, pid + 8);
  CheckReferencesVp9(pid + 10, 0, pid + 8);
  CheckReferencesVp9(pid + 11, 0, pid + 10);
  CheckReferencesVp9(pid + 12, 0, pid + 10);
  CheckReferencesVp9(pid + 13, 0, pid + 12);
  CheckReferencesVp9(pid + 14, 0, pid + 12);
  CheckReferencesVp9(pid + 15, 0, pid + 14);
  CheckReferencesVp9(pid + 16, 0, pid + 14);
  CheckReferencesVp9(pid + 17, 0, pid + 16);
  CheckReferencesVp9(pid + 18, 0, pid + 16);
  CheckReferencesVp9(pid + 19, 0, pid + 18);
}

TEST_F(TestPacketBuffer, Vp9GofTemporalLayersReordered_01) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode2);  // 01 pattern

  //           sn     , kf, frst, lst, up, pid     , sid, tid, tl0, ss
  InsertVp9Gof(sn + 1 , kF, kT  , kT , kF, pid + 1 , 0  , 1  , 0);
  InsertVp9Gof(sn     , kT, kT  , kT , kF, pid     , 0  , 0  , 0  , &ss);
  InsertVp9Gof(sn + 2 , kF, kT  , kT , kF, pid + 2 , 0  , 0  , 1);
  InsertVp9Gof(sn + 4 , kF, kT  , kT , kF, pid + 4 , 0  , 0  , 2);
  InsertVp9Gof(sn + 3 , kF, kT  , kT , kF, pid + 3 , 0  , 1  , 1);
  InsertVp9Gof(sn + 5 , kF, kT  , kT , kF, pid + 5 , 0  , 1  , 2);
  InsertVp9Gof(sn + 7 , kF, kT  , kT , kF, pid + 7 , 0  , 1  , 3);
  InsertVp9Gof(sn + 6 , kF, kT  , kT , kF, pid + 6 , 0  , 0  , 3);
  InsertVp9Gof(sn + 10, kF, kT  , kT , kF, pid + 10, 0  , 0  , 5);
  InsertVp9Gof(sn + 8 , kF, kT  , kT , kF, pid + 8 , 0  , 0  , 4);
  InsertVp9Gof(sn + 9 , kF, kT  , kT , kF, pid + 9 , 0  , 1  , 4);
  InsertVp9Gof(sn + 11, kF, kT  , kT , kF, pid + 11, 0  , 1  , 5);
  InsertVp9Gof(sn + 13, kF, kT  , kT , kF, pid + 13, 0  , 1  , 6);
  InsertVp9Gof(sn + 16, kF, kT  , kT , kF, pid + 16, 0  , 0  , 8);
  InsertVp9Gof(sn + 12, kF, kT  , kT , kF, pid + 12, 0  , 0  , 6);
  InsertVp9Gof(sn + 14, kF, kT  , kT , kF, pid + 14, 0  , 0  , 7);
  InsertVp9Gof(sn + 17, kF, kT  , kT , kF, pid + 17, 0  , 1  , 8);
  InsertVp9Gof(sn + 19, kF, kT  , kT , kF, pid + 19, 0  , 1  , 9);
  InsertVp9Gof(sn + 15, kF, kT  , kT , kF, pid + 15, 0  , 1  , 7);
  InsertVp9Gof(sn + 18, kF, kT  , kT , kF, pid + 18, 0  , 0  , 9);

  ASSERT_EQ(20UL, frames_from_callback_.size());
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid + 1 , 0, pid);
  CheckReferencesVp9(pid + 2 , 0, pid);
  CheckReferencesVp9(pid + 3 , 0, pid + 2);
  CheckReferencesVp9(pid + 4 , 0, pid + 2);
  CheckReferencesVp9(pid + 5 , 0, pid + 4);
  CheckReferencesVp9(pid + 6 , 0, pid + 4);
  CheckReferencesVp9(pid + 7 , 0, pid + 6);
  CheckReferencesVp9(pid + 8 , 0, pid + 6);
  CheckReferencesVp9(pid + 9 , 0, pid + 8);
  CheckReferencesVp9(pid + 10, 0, pid + 8);
  CheckReferencesVp9(pid + 11, 0, pid + 10);
  CheckReferencesVp9(pid + 12, 0, pid + 10);
  CheckReferencesVp9(pid + 13, 0, pid + 12);
  CheckReferencesVp9(pid + 14, 0, pid + 12);
  CheckReferencesVp9(pid + 15, 0, pid + 14);
  CheckReferencesVp9(pid + 16, 0, pid + 14);
  CheckReferencesVp9(pid + 17, 0, pid + 16);
  CheckReferencesVp9(pid + 18, 0, pid + 16);
  CheckReferencesVp9(pid + 19, 0, pid + 18);
}

TEST_F(TestPacketBuffer, Vp9GofTemporalLayers_0212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode3);  // 0212 pattern

  //           sn     , kf, frst, lst, up, pid     , sid, tid, tl0, ss
  InsertVp9Gof(sn     , kT, kT  , kT , kF, pid     , 0  , 0  , 0  , &ss);
  InsertVp9Gof(sn + 1 , kF, kT  , kT , kF, pid + 1 , 0  , 2  , 0);
  InsertVp9Gof(sn + 2 , kF, kT  , kT , kF, pid + 2 , 0  , 1  , 0);
  InsertVp9Gof(sn + 3 , kF, kT  , kT , kF, pid + 3 , 0  , 2  , 0);
  InsertVp9Gof(sn + 4 , kF, kT  , kT , kF, pid + 4 , 0  , 0  , 1);
  InsertVp9Gof(sn + 5 , kF, kT  , kT , kF, pid + 5 , 0  , 2  , 1);
  InsertVp9Gof(sn + 6 , kF, kT  , kT , kF, pid + 6 , 0  , 1  , 1);
  InsertVp9Gof(sn + 7 , kF, kT  , kT , kF, pid + 7 , 0  , 2  , 1);
  InsertVp9Gof(sn + 8 , kF, kT  , kT , kF, pid + 8 , 0  , 0  , 2);
  InsertVp9Gof(sn + 9 , kF, kT  , kT , kF, pid + 9 , 0  , 2  , 2);
  InsertVp9Gof(sn + 10, kF, kT  , kT , kF, pid + 10, 0  , 1  , 2);
  InsertVp9Gof(sn + 11, kF, kT  , kT , kF, pid + 11, 0  , 2  , 2);
  InsertVp9Gof(sn + 12, kF, kT  , kT , kF, pid + 12, 0  , 0  , 3);
  InsertVp9Gof(sn + 13, kF, kT  , kT , kF, pid + 13, 0  , 2  , 3);
  InsertVp9Gof(sn + 14, kF, kT  , kT , kF, pid + 14, 0  , 1  , 3);
  InsertVp9Gof(sn + 15, kF, kT  , kT , kF, pid + 15, 0  , 2  , 3);
  InsertVp9Gof(sn + 16, kF, kT  , kT , kF, pid + 16, 0  , 0  , 4);
  InsertVp9Gof(sn + 17, kF, kT  , kT , kF, pid + 17, 0  , 2  , 4);
  InsertVp9Gof(sn + 18, kF, kT  , kT , kF, pid + 18, 0  , 1  , 4);
  InsertVp9Gof(sn + 19, kF, kT  , kT , kF, pid + 19, 0  , 2  , 4);

  ASSERT_EQ(20UL, frames_from_callback_.size());
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid + 1 , 0, pid);
  CheckReferencesVp9(pid + 2 , 0, pid);
  CheckReferencesVp9(pid + 3 , 0, pid + 1, pid + 2);
  CheckReferencesVp9(pid + 4 , 0, pid);
  CheckReferencesVp9(pid + 5 , 0, pid + 4);
  CheckReferencesVp9(pid + 6 , 0, pid + 4);
  CheckReferencesVp9(pid + 7 , 0, pid + 5, pid + 6);
  CheckReferencesVp9(pid + 8 , 0, pid + 4);
  CheckReferencesVp9(pid + 9 , 0, pid + 8);
  CheckReferencesVp9(pid + 10, 0, pid + 8);
  CheckReferencesVp9(pid + 11, 0, pid + 9, pid + 10);
  CheckReferencesVp9(pid + 12, 0, pid + 8);
  CheckReferencesVp9(pid + 13, 0, pid + 12);
  CheckReferencesVp9(pid + 14, 0, pid + 12);
  CheckReferencesVp9(pid + 15, 0, pid + 13, pid + 14);
  CheckReferencesVp9(pid + 16, 0, pid + 12);
  CheckReferencesVp9(pid + 17, 0, pid + 16);
  CheckReferencesVp9(pid + 18, 0, pid + 16);
  CheckReferencesVp9(pid + 19, 0, pid + 17, pid + 18);
}

TEST_F(TestPacketBuffer, Vp9GofTemporalLayersReordered_0212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode3);  // 0212 pattern

  //           sn     , kf, frst, lst, up, pid     , sid, tid, tl0, ss
  InsertVp9Gof(sn + 2 , kF, kT  , kT , kF, pid + 2 , 0  , 1  , 0);
  InsertVp9Gof(sn + 1 , kF, kT  , kT , kF, pid + 1 , 0  , 2  , 0);
  InsertVp9Gof(sn     , kT, kT  , kT , kF, pid     , 0  , 0  , 0  , &ss);
  InsertVp9Gof(sn + 3 , kF, kT  , kT , kF, pid + 3 , 0  , 2  , 0);
  InsertVp9Gof(sn + 6 , kF, kT  , kT , kF, pid + 6 , 0  , 1  , 1);
  InsertVp9Gof(sn + 5 , kF, kT  , kT , kF, pid + 5 , 0  , 2  , 1);
  InsertVp9Gof(sn + 4 , kF, kT  , kT , kF, pid + 4 , 0  , 0  , 1);
  InsertVp9Gof(sn + 9 , kF, kT  , kT , kF, pid + 9 , 0  , 2  , 2);
  InsertVp9Gof(sn + 7 , kF, kT  , kT , kF, pid + 7 , 0  , 2  , 1);
  InsertVp9Gof(sn + 8 , kF, kT  , kT , kF, pid + 8 , 0  , 0  , 2);
  InsertVp9Gof(sn + 11, kF, kT  , kT , kF, pid + 11, 0  , 2  , 2);
  InsertVp9Gof(sn + 10, kF, kT  , kT , kF, pid + 10, 0  , 1  , 2);
  InsertVp9Gof(sn + 13, kF, kT  , kT , kF, pid + 13, 0  , 2  , 3);
  InsertVp9Gof(sn + 12, kF, kT  , kT , kF, pid + 12, 0  , 0  , 3);
  InsertVp9Gof(sn + 14, kF, kT  , kT , kF, pid + 14, 0  , 1  , 3);
  InsertVp9Gof(sn + 16, kF, kT  , kT , kF, pid + 16, 0  , 0  , 4);
  InsertVp9Gof(sn + 15, kF, kT  , kT , kF, pid + 15, 0  , 2  , 3);
  InsertVp9Gof(sn + 17, kF, kT  , kT , kF, pid + 17, 0  , 2  , 4);
  InsertVp9Gof(sn + 19, kF, kT  , kT , kF, pid + 19, 0  , 2  , 4);
  InsertVp9Gof(sn + 18, kF, kT  , kT , kF, pid + 18, 0  , 1  , 4);

  ASSERT_EQ(20UL, frames_from_callback_.size());
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid + 1 , 0, pid);
  CheckReferencesVp9(pid + 2 , 0, pid);
  CheckReferencesVp9(pid + 3 , 0, pid + 1, pid + 2);
  CheckReferencesVp9(pid + 4 , 0, pid);
  CheckReferencesVp9(pid + 5 , 0, pid + 4);
  CheckReferencesVp9(pid + 6 , 0, pid + 4);
  CheckReferencesVp9(pid + 7 , 0, pid + 5, pid + 6);
  CheckReferencesVp9(pid + 8 , 0, pid + 4);
  CheckReferencesVp9(pid + 9 , 0, pid + 8);
  CheckReferencesVp9(pid + 10, 0, pid + 8);
  CheckReferencesVp9(pid + 11, 0, pid + 9, pid + 10);
  CheckReferencesVp9(pid + 12, 0, pid + 8);
  CheckReferencesVp9(pid + 13, 0, pid + 12);
  CheckReferencesVp9(pid + 14, 0, pid + 12);
  CheckReferencesVp9(pid + 15, 0, pid + 13, pid + 14);
  CheckReferencesVp9(pid + 16, 0, pid + 12);
  CheckReferencesVp9(pid + 17, 0, pid + 16);
  CheckReferencesVp9(pid + 18, 0, pid + 16);
  CheckReferencesVp9(pid + 19, 0, pid + 17, pid + 18);
}

TEST_F(TestPacketBuffer, Vp9GofTemporalLayersUpSwitch_02120212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode4);  // 02120212 pattern

  //           sn     , kf, frst, lst, up, pid     , sid, tid, tl0, ss
  InsertVp9Gof(sn     , kT, kT  , kT , kF, pid     , 0  , 0  , 0  , &ss);
  InsertVp9Gof(sn + 1 , kF, kT  , kT , kF, pid + 1 , 0  , 2  , 0);
  InsertVp9Gof(sn + 2 , kF, kT  , kT , kF, pid + 2 , 0  , 1  , 0);
  InsertVp9Gof(sn + 3 , kF, kT  , kT , kF, pid + 3 , 0  , 2  , 0);
  InsertVp9Gof(sn + 4 , kF, kT  , kT , kF, pid + 4 , 0  , 0  , 1);
  InsertVp9Gof(sn + 5 , kF, kT  , kT , kF, pid + 5 , 0  , 2  , 1);
  InsertVp9Gof(sn + 6 , kF, kT  , kT , kT, pid + 6 , 0  , 1  , 1);
  InsertVp9Gof(sn + 7 , kF, kT  , kT , kF, pid + 7 , 0  , 2  , 1);
  InsertVp9Gof(sn + 8 , kF, kT  , kT , kT, pid + 8 , 0  , 0  , 2);
  InsertVp9Gof(sn + 9 , kF, kT  , kT , kF, pid + 9 , 0  , 2  , 2);
  InsertVp9Gof(sn + 10, kF, kT  , kT , kF, pid + 10, 0  , 1  , 2);
  InsertVp9Gof(sn + 11, kF, kT  , kT , kT, pid + 11, 0  , 2  , 2);
  InsertVp9Gof(sn + 12, kF, kT  , kT , kF, pid + 12, 0  , 0  , 3);
  InsertVp9Gof(sn + 13, kF, kT  , kT , kF, pid + 13, 0  , 2  , 3);
  InsertVp9Gof(sn + 14, kF, kT  , kT , kF, pid + 14, 0  , 1  , 3);
  InsertVp9Gof(sn + 15, kF, kT  , kT , kF, pid + 15, 0  , 2  , 3);

  ASSERT_EQ(16UL, frames_from_callback_.size());
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid + 1 , 0, pid);
  CheckReferencesVp9(pid + 2 , 0, pid);
  CheckReferencesVp9(pid + 3 , 0, pid + 1, pid + 2);
  CheckReferencesVp9(pid + 4 , 0, pid);
  CheckReferencesVp9(pid + 5 , 0, pid + 3, pid + 4);
  CheckReferencesVp9(pid + 6 , 0, pid + 2, pid + 4);
  CheckReferencesVp9(pid + 7 , 0, pid + 6);
  CheckReferencesVp9(pid + 8 , 0, pid + 4);
  CheckReferencesVp9(pid + 9 , 0, pid + 8);
  CheckReferencesVp9(pid + 10, 0, pid + 8);
  CheckReferencesVp9(pid + 11, 0, pid + 9, pid + 10);
  CheckReferencesVp9(pid + 12, 0, pid + 8);
  CheckReferencesVp9(pid + 13, 0, pid + 11, pid + 12);
  CheckReferencesVp9(pid + 14, 0, pid + 10, pid + 12);
  CheckReferencesVp9(pid + 15, 0, pid + 13, pid + 14);
}

TEST_F(TestPacketBuffer, Vp9GofTemporalLayersUpSwitchReordered_02120212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode4);  // 02120212 pattern

  //           sn     , kf, frst, lst, up, pid     , sid, tid, tl0, ss
  InsertVp9Gof(sn + 1 , kF, kT  , kT , kF, pid + 1 , 0  , 2  , 0);
  InsertVp9Gof(sn     , kT, kT  , kT , kF, pid     , 0  , 0  , 0  , &ss);
  InsertVp9Gof(sn + 4 , kF, kT  , kT , kF, pid + 4 , 0  , 0  , 1);
  InsertVp9Gof(sn + 2 , kF, kT  , kT , kF, pid + 2 , 0  , 1  , 0);
  InsertVp9Gof(sn + 5 , kF, kT  , kT , kF, pid + 5 , 0  , 2  , 1);
  InsertVp9Gof(sn + 3 , kF, kT  , kT , kF, pid + 3 , 0  , 2  , 0);
  InsertVp9Gof(sn + 7 , kF, kT  , kT , kF, pid + 7 , 0  , 2  , 1);
  InsertVp9Gof(sn + 9 , kF, kT  , kT , kF, pid + 9 , 0  , 2  , 2);
  InsertVp9Gof(sn + 6 , kF, kT  , kT , kT, pid + 6 , 0  , 1  , 1);
  InsertVp9Gof(sn + 12, kF, kT  , kT , kF, pid + 12, 0  , 0  , 3);
  InsertVp9Gof(sn + 10, kF, kT  , kT , kF, pid + 10, 0  , 1  , 2);
  InsertVp9Gof(sn + 8 , kF, kT  , kT , kT, pid + 8 , 0  , 0  , 2);
  InsertVp9Gof(sn + 11, kF, kT  , kT , kT, pid + 11, 0  , 2  , 2);
  InsertVp9Gof(sn + 13, kF, kT  , kT , kF, pid + 13, 0  , 2  , 3);
  InsertVp9Gof(sn + 15, kF, kT  , kT , kF, pid + 15, 0  , 2  , 3);
  InsertVp9Gof(sn + 14, kF, kT  , kT , kF, pid + 14, 0  , 1  , 3);

  ASSERT_EQ(16UL, frames_from_callback_.size());
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid + 1 , 0, pid);
  CheckReferencesVp9(pid + 2 , 0, pid);
  CheckReferencesVp9(pid + 3 , 0, pid + 1, pid + 2);
  CheckReferencesVp9(pid + 4 , 0, pid);
  CheckReferencesVp9(pid + 5 , 0, pid + 3, pid + 4);
  CheckReferencesVp9(pid + 6 , 0, pid + 2, pid + 4);
  CheckReferencesVp9(pid + 7 , 0, pid + 6);
  CheckReferencesVp9(pid + 8 , 0, pid + 4);
  CheckReferencesVp9(pid + 9 , 0, pid + 8);
  CheckReferencesVp9(pid + 10, 0, pid + 8);
  CheckReferencesVp9(pid + 11, 0, pid + 9, pid + 10);
  CheckReferencesVp9(pid + 12, 0, pid + 8);
  CheckReferencesVp9(pid + 13, 0, pid + 11, pid + 12);
  CheckReferencesVp9(pid + 14, 0, pid + 10, pid + 12);
  CheckReferencesVp9(pid + 15, 0, pid + 13, pid + 14);
}

TEST_F(TestPacketBuffer, Vp9GofTemporalLayersReordered_01_0212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode2);  // 01 pattern

  //           sn     , kf, frst, lst, up, pid     , sid, tid, tl0, ss
  InsertVp9Gof(sn + 1 , kF, kT  , kT , kF, pid + 1 , 0  , 1  , 0);
  InsertVp9Gof(sn     , kT, kT  , kT , kF, pid     , 0  , 0  , 0  , &ss);
  InsertVp9Gof(sn + 3 , kF, kT  , kT , kF, pid + 3 , 0  , 1  , 1);
  InsertVp9Gof(sn + 6 , kF, kT  , kT , kF, pid + 6 , 0  , 1  , 2);
  ss.SetGofInfoVP9(kTemporalStructureMode3);  // 0212 pattern
  InsertVp9Gof(sn + 4 , kF, kT  , kT , kF, pid + 4 , 0  , 0  , 2  , &ss);
  InsertVp9Gof(sn + 2 , kF, kT  , kT , kF, pid + 2 , 0  , 0  , 1);
  InsertVp9Gof(sn + 5 , kF, kT  , kT , kF, pid + 5 , 0  , 2  , 2);
  InsertVp9Gof(sn + 8 , kF, kT  , kT , kF, pid + 8 , 0  , 0  , 3);
  InsertVp9Gof(sn + 10, kF, kT  , kT , kF, pid + 10, 0  , 1  , 3);
  InsertVp9Gof(sn + 7 , kF, kT  , kT , kF, pid + 7 , 0  , 2  , 2);
  InsertVp9Gof(sn + 11, kF, kT  , kT , kF, pid + 11, 0  , 2  , 3);
  InsertVp9Gof(sn + 9 , kF, kT  , kT , kF, pid + 9 , 0  , 2  , 3);

  ASSERT_EQ(12UL, frames_from_callback_.size());
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid + 1 , 0, pid);
  CheckReferencesVp9(pid + 2 , 0, pid);
  CheckReferencesVp9(pid + 3 , 0, pid + 2);
  CheckReferencesVp9(pid + 4 , 0, pid);
  CheckReferencesVp9(pid + 5 , 0, pid + 4);
  CheckReferencesVp9(pid + 6 , 0, pid + 4);
  CheckReferencesVp9(pid + 7 , 0, pid + 5, pid + 6);
  CheckReferencesVp9(pid + 8 , 0, pid + 4);
  CheckReferencesVp9(pid + 9 , 0, pid + 8);
  CheckReferencesVp9(pid + 10, 0, pid + 8);
  CheckReferencesVp9(pid + 11, 0, pid + 9, pid + 10);
}

TEST_F(TestPacketBuffer, Vp9FlexibleModeOneFrame) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  //            sn, kf, frst, lst, intr, pid, sid, tid, tl0
  InsertVp9Flex(sn, kT, kT  , kT , kF  , pid, 0  , 0  , 0);

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckReferencesVp9(pid, 0);
}

TEST_F(TestPacketBuffer, Vp9FlexibleModeTwoSpatialLayers) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  //            sn     , kf, frst, lst, intr, pid    , sid, tid, tl0, refs
  InsertVp9Flex(sn     , kT, kT  , kT , kF  , pid    , 0  , 0  , 0);
  InsertVp9Flex(sn + 1 , kT, kT  , kT , kT  , pid    , 1  , 0  , 0);
  InsertVp9Flex(sn + 2 , kF, kT  , kT , kF  , pid + 1, 1  , 0  , 0  , {1});
  InsertVp9Flex(sn + 3 , kF, kT  , kT , kF  , pid + 2, 0  , 0  , 1  , {2});
  InsertVp9Flex(sn + 4 , kF, kT  , kT , kF  , pid + 2, 1  , 0  , 1  , {1});
  InsertVp9Flex(sn + 5 , kF, kT  , kT , kF  , pid + 3, 1  , 0  , 1  , {1});
  InsertVp9Flex(sn + 6 , kF, kT  , kT , kF  , pid + 4, 0  , 0  , 2  , {2});
  InsertVp9Flex(sn + 7 , kF, kT  , kT , kF  , pid + 4, 1  , 0  , 2  , {1});
  InsertVp9Flex(sn + 8 , kF, kT  , kT , kF  , pid + 5, 1  , 0  , 2  , {1});
  InsertVp9Flex(sn + 9 , kF, kT  , kT , kF  , pid + 6, 0  , 0  , 3  , {2});
  InsertVp9Flex(sn + 10, kF, kT  , kT , kF  , pid + 6, 1  , 0  , 3  , {1});
  InsertVp9Flex(sn + 11, kF, kT  , kT , kF  , pid + 7, 1  , 0  , 3  , {1});
  InsertVp9Flex(sn + 12, kF, kT  , kT , kF  , pid + 8, 0  , 0  , 4  , {2});
  InsertVp9Flex(sn + 13, kF, kT  , kT , kF  , pid + 8, 1  , 0  , 4  , {1});

  ASSERT_EQ(14UL, frames_from_callback_.size());
  CheckReferencesVp9(pid    , 0);
  CheckReferencesVp9(pid    , 1);
  CheckReferencesVp9(pid + 1, 1, pid);
  CheckReferencesVp9(pid + 2, 0, pid);
  CheckReferencesVp9(pid + 2, 1, pid + 1);
  CheckReferencesVp9(pid + 3, 1, pid + 2);
  CheckReferencesVp9(pid + 4, 0, pid + 2);
  CheckReferencesVp9(pid + 4, 1, pid + 3);
  CheckReferencesVp9(pid + 5, 1, pid + 4);
  CheckReferencesVp9(pid + 6, 0, pid + 4);
  CheckReferencesVp9(pid + 6, 1, pid + 5);
  CheckReferencesVp9(pid + 7, 1, pid + 6);
  CheckReferencesVp9(pid + 8, 0, pid + 6);
  CheckReferencesVp9(pid + 8, 1, pid + 7);
}

TEST_F(TestPacketBuffer, Vp9FlexibleModeTwoSpatialLayersReordered) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  //            sn     , kf, frst, lst, intr, pid    , sid, tid, tl0, refs
  InsertVp9Flex(sn + 1 , kT, kT  , kT , kT  , pid    , 1  , 0  , 0);
  InsertVp9Flex(sn + 2 , kF, kT  , kT , kF  , pid + 1, 1  , 0  , 0  , {1});
  InsertVp9Flex(sn     , kT, kT  , kT , kF  , pid    , 0  , 0  , 0);
  InsertVp9Flex(sn + 4 , kF, kT  , kT , kF  , pid + 2, 1  , 0  , 1  , {1});
  InsertVp9Flex(sn + 5 , kF, kT  , kT , kF  , pid + 3, 1  , 0  , 1  , {1});
  InsertVp9Flex(sn + 3 , kF, kT  , kT , kF  , pid + 2, 0  , 0  , 1  , {2});
  InsertVp9Flex(sn + 7 , kF, kT  , kT , kF  , pid + 4, 1  , 0  , 2  , {1});
  InsertVp9Flex(sn + 6 , kF, kT  , kT , kF  , pid + 4, 0  , 0  , 2  , {2});
  InsertVp9Flex(sn + 8 , kF, kT  , kT , kF  , pid + 5, 1  , 0  , 2  , {1});
  InsertVp9Flex(sn + 9 , kF, kT  , kT , kF  , pid + 6, 0  , 0  , 3  , {2});
  InsertVp9Flex(sn + 11, kF, kT  , kT , kF  , pid + 7, 1  , 0  , 3  , {1});
  InsertVp9Flex(sn + 10, kF, kT  , kT , kF  , pid + 6, 1  , 0  , 3  , {1});
  InsertVp9Flex(sn + 13, kF, kT  , kT , kF  , pid + 8, 1  , 0  , 4  , {1});
  InsertVp9Flex(sn + 12, kF, kT  , kT , kF  , pid + 8, 0  , 0  , 4  , {2});

  ASSERT_EQ(14UL, frames_from_callback_.size());
  CheckReferencesVp9(pid    , 0);
  CheckReferencesVp9(pid    , 1);
  CheckReferencesVp9(pid + 1, 1, pid);
  CheckReferencesVp9(pid + 2, 0, pid);
  CheckReferencesVp9(pid + 2, 1, pid + 1);
  CheckReferencesVp9(pid + 3, 1, pid + 2);
  CheckReferencesVp9(pid + 4, 0, pid + 2);
  CheckReferencesVp9(pid + 4, 1, pid + 3);
  CheckReferencesVp9(pid + 5, 1, pid + 4);
  CheckReferencesVp9(pid + 6, 0, pid + 4);
  CheckReferencesVp9(pid + 6, 1, pid + 5);
  CheckReferencesVp9(pid + 7, 1, pid + 6);
  CheckReferencesVp9(pid + 8, 0, pid + 6);
  CheckReferencesVp9(pid + 8, 1, pid + 7);
}

}  // namespace video_coding
}  // namespace webrtc
