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
#include "modules/video_coding/main/source/jitter_buffer.h"
#include "modules/video_coding/main/source/media_opt_util.h"
#include "modules/video_coding/main/source/mock/fake_tick_time.h"
#include "modules/video_coding/main/source/packet.h"

namespace webrtc {

class StreamGenerator {
 public:
  StreamGenerator(uint16_t start_seq_num, uint32_t start_timestamp,
                  int64_t current_time)
      : sequence_number_(start_seq_num),
        timestamp_(start_timestamp),
        start_time_(current_time),
        num_packets_(0),
        type_(kVideoFrameKey),
        first_packet_(true) {}

  void GenerateFrame(FrameType type, int num_packets, int64_t current_time) {
    timestamp_ += 90 * (current_time - start_time_);
    // Move the sequence number counter if all packets from the previous frame
    // wasn't collected.
    sequence_number_ += num_packets_;
    num_packets_ = num_packets;
    type_ = type;
    first_packet_ = true;
  }

  bool NextPacket(VCMPacket* packet) {
    if (num_packets_ == 0) {
      return false;
    }
    --num_packets_;
    if (packet) {
      packet->seqNum = sequence_number_;
      packet->timestamp = timestamp_;
      packet->frameType = type_;
      packet->isFirstPacket = first_packet_;
      packet->markerBit = (num_packets_ == 0);
      if (packet->isFirstPacket)
        packet->completeNALU = kNaluStart;
      else if (packet->markerBit)
        packet->completeNALU = kNaluEnd;
      else
        packet->completeNALU = kNaluIncomplete;
    }
    ++sequence_number_;
    first_packet_ = false;
    return true;
  }

  int PacketsRemaining() const {
    return num_packets_;
  }

 private:
  uint16_t sequence_number_;
  uint32_t timestamp_;
  int64_t start_time_;
  int num_packets_;
  FrameType type_;
  bool first_packet_;

  DISALLOW_COPY_AND_ASSIGN(StreamGenerator);
};

class TestRunningJitterBuffer : public ::testing::Test {
 protected:
  enum { kDataBufferSize = 10 };
  enum { kDefaultFrameRate = 25 };
  enum { kDefaultFramePeriodMs = 1000 / kDefaultFrameRate };

  virtual void SetUp() {
    clock_ = new FakeTickTime(0);
    jitter_buffer_ = new VCMJitterBuffer(clock_);
    stream_generator = new StreamGenerator(0, 0,
                                           clock_->MillisecondTimestamp());
    jitter_buffer_->Start();
    memset(data_buffer_, 0, kDataBufferSize);
  }

  virtual void TearDown() {
    jitter_buffer_->Stop();
    delete stream_generator;
    delete jitter_buffer_;
    delete clock_;
  }

  VCMFrameBufferEnum InsertNextPacket() {
    VCMPacket packet;
    packet.dataPtr = data_buffer_;
    VCMEncodedFrame* frame;
    bool packet_available = stream_generator->NextPacket(&packet);
    EXPECT_TRUE(packet_available);
    if (!packet_available)
      return kStateError;  // Return here to avoid crashes below.
    EXPECT_EQ(VCM_OK, jitter_buffer_->GetFrame(packet, frame));
    return jitter_buffer_->InsertPacket(frame, packet);
  }

  void InsertFrame(FrameType frame_type) {
    stream_generator->GenerateFrame(frame_type, 1,
                                    clock_->MillisecondTimestamp());
    EXPECT_EQ(kFirstPacket, InsertNextPacket());
    clock_->IncrementDebugClock(kDefaultFramePeriodMs);
  }

  void InsertFrames(int num_frames) {
    for (int i = 0; i < num_frames; ++i) {
      InsertFrame(kVideoFrameDelta);
    }
  }

  void DropFrame(int num_packets) {
    stream_generator->GenerateFrame(kVideoFrameDelta, num_packets,
                                    clock_->MillisecondTimestamp());
    clock_->IncrementDebugClock(kDefaultFramePeriodMs);
  }

  VCMJitterBuffer* jitter_buffer_;
  StreamGenerator* stream_generator;
  FakeTickTime* clock_;
  uint8_t data_buffer_[kDataBufferSize];
};

class TestNack : public TestRunningJitterBuffer {
 protected:
  virtual void SetUp() {
    TestRunningJitterBuffer::SetUp();
    jitter_buffer_->SetNackMode(kNackInfinite, -1, -1);
  }

  virtual void TearDown() {
    TestRunningJitterBuffer::TearDown();
  }
};

TEST_F(TestNack, TestJitterBufferFull) {
  // Insert a key frame and decode it.
  InsertFrame(kVideoFrameKey);
  EXPECT_TRUE(jitter_buffer_->GetCompleteFrameForDecoding(0) != NULL);
  DropFrame(1);
  // Fill the jitter buffer.
  InsertFrames(kMaxNumberOfFrames);
  // Make sure we can't decode these frames.
  EXPECT_TRUE(jitter_buffer_->GetCompleteFrameForDecoding(0) == NULL);
  // This frame will make the jitter buffer recycle frames until a key frame.
  // Since none is found it will have to wait until the next key frame before
  // decoding.
  InsertFrame(kVideoFrameDelta);
  EXPECT_TRUE(jitter_buffer_->GetCompleteFrameForDecoding(0) == NULL);
}

TEST_F(TestNack, TestNackListFull) {
  // Insert a key frame and decode it.
  InsertFrame(kVideoFrameKey);
  EXPECT_TRUE(jitter_buffer_->GetCompleteFrameForDecoding(0) != NULL);

  // Generate and drop |kNackHistoryLength| packets to fill the NACK list.
  DropFrame(kNackHistoryLength);
  // Insert a frame which should trigger a recycle until the next key frame.
  InsertFrame(kVideoFrameDelta);
  EXPECT_TRUE(jitter_buffer_->GetCompleteFrameForDecoding(0) == NULL);

  uint16_t nack_list_length = kNackHistoryLength;
  bool extended;
  uint16_t* nack_list = jitter_buffer_->GetNackList(nack_list_length, extended);
  // Verify that the jitter buffer requests a key frame.
  EXPECT_TRUE(nack_list_length == 0xffff && nack_list == NULL);

  InsertFrame(kVideoFrameDelta);
  EXPECT_TRUE(jitter_buffer_->GetCompleteFrameForDecoding(0) == NULL);
  EXPECT_TRUE(jitter_buffer_->GetFrameForDecoding() == NULL);
}

TEST_F(TestNack, TestNackBeforeDecode) {
  DropFrame(10);
  // Insert a frame and try to generate a NACK list. Shouldn't get one.
  InsertFrame(kVideoFrameDelta);
  uint16_t nack_list_size = 0;
  bool extended = false;
  uint16_t* list = jitter_buffer_->GetNackList(nack_list_size, extended);
  // No list generated, and a key frame request is signaled.
  EXPECT_TRUE(list == NULL);
  EXPECT_TRUE(nack_list_size == 0xFFFF);
}

TEST_F(TestNack, TestNormalOperation) {
  EXPECT_EQ(kNackInfinite, jitter_buffer_->GetNackMode());

  InsertFrame(kVideoFrameKey);
  EXPECT_TRUE(jitter_buffer_->GetCompleteFrameForDecoding(0) != NULL);

  //  ----------------------------------------------------------------
  // | 1 | 2 | .. | 8 | 9 | x | 11 | 12 | .. | 19 | x | 21 | .. | 100 |
  //  ----------------------------------------------------------------
  stream_generator->GenerateFrame(kVideoFrameKey, 100,
                                  clock_->MillisecondTimestamp());
  clock_->IncrementDebugClock(kDefaultFramePeriodMs);
  EXPECT_EQ(kFirstPacket, InsertNextPacket());
  // Verify that the frame is incomplete.
  EXPECT_TRUE(jitter_buffer_->GetCompleteFrameForDecoding(0) == NULL);
  int i = 2;
  while (stream_generator->PacketsRemaining() > 1) {
    if (i % 10 != 0)
      EXPECT_EQ(kIncomplete, InsertNextPacket());
    else
      stream_generator->NextPacket(NULL);  // Drop packet
    ++i;
  }
  EXPECT_EQ(kIncomplete, InsertNextPacket());
  EXPECT_EQ(0, stream_generator->PacketsRemaining());
  EXPECT_TRUE(jitter_buffer_->GetCompleteFrameForDecoding(0) == NULL);
  EXPECT_TRUE(jitter_buffer_->GetFrameForDecoding() == NULL);
  uint16_t nack_list_size = 0;
  bool extended = false;
  uint16_t* list = jitter_buffer_->GetNackList(nack_list_size, extended);
  // Verify the NACK list.
  const int kExpectedNackSize = 9;
  ASSERT_EQ(kExpectedNackSize, nack_list_size);
  for (i = 0; i < nack_list_size; ++i)
    EXPECT_EQ((1 + i) * 10, list[i]);
}

}  // namespace webrtc
