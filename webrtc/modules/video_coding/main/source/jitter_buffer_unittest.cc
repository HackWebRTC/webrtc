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

#include <list>

#include "gtest/gtest.h"
#include "modules/video_coding/main/source/jitter_buffer.h"
#include "modules/video_coding/main/source/media_opt_util.h"
#include "modules/video_coding/main/source/packet.h"
#include "webrtc/system_wrappers/interface/clock.h"

namespace webrtc {

enum { kDefaultFrameRate = 25u };
enum { kDefaultFramePeriodMs = 1000u / kDefaultFrameRate };
const unsigned int kDefaultBitrateKbps = 1000;
const unsigned int kFrameSize = (kDefaultBitrateKbps + kDefaultFrameRate * 4) /
    (kDefaultFrameRate * 8);
const unsigned int kMaxPacketSize = 1500;

class StreamGenerator {
 public:
  StreamGenerator(uint16_t start_seq_num, uint32_t start_timestamp,
                  int64_t current_time)
      : packets_(),
        sequence_number_(start_seq_num),
        timestamp_(start_timestamp),
        start_time_(current_time) {}

  void Init(uint16_t start_seq_num, uint32_t start_timestamp,
            int64_t current_time) {
    packets_.clear();
    sequence_number_ = start_seq_num;
    timestamp_ = start_timestamp;
    start_time_ = current_time;
    memset(&packet_buffer, 0, sizeof(packet_buffer));
  }

  void GenerateFrame(FrameType type, int num_media_packets,
                     int num_empty_packets, int64_t current_time) {
    timestamp_ += 90 * (current_time - start_time_);
    // Move the sequence number counter if all packets from the previous frame
    // wasn't collected.
    sequence_number_ += packets_.size();
    packets_.clear();
    for (int i = 0; i < num_media_packets; ++i) {
      const int packet_size = (kFrameSize + num_media_packets / 2) /
          num_media_packets;
      packets_.push_back(GeneratePacket(sequence_number_,
                                        timestamp_,
                                        packet_size,
                                        (i == 0),
                                        (i == num_media_packets - 1),
                                        type));
      ++sequence_number_;
    }
    for (int i = 0; i < num_empty_packets; ++i) {
      packets_.push_back(GeneratePacket(sequence_number_,
                                        timestamp_,
                                        0,
                                        false,
                                        false,
                                        kFrameEmpty));
      ++sequence_number_;
    }
  }

  VCMPacket GeneratePacket(uint16_t sequence_number,
                           uint32_t timestamp,
                           unsigned int size,
                           bool first_packet,
                           bool marker_bit,
                           FrameType type) {
    EXPECT_LT(size, kMaxPacketSize);
    VCMPacket packet;
    packet.seqNum = sequence_number;
    packet.timestamp = timestamp;
    packet.frameType = type;
    packet.isFirstPacket = first_packet;
    packet.markerBit = marker_bit;
    packet.sizeBytes = size;
    packet.dataPtr = packet_buffer;
    if (packet.isFirstPacket)
      packet.completeNALU = kNaluStart;
    else if (packet.markerBit)
      packet.completeNALU = kNaluEnd;
    else
      packet.completeNALU = kNaluIncomplete;
    return packet;
  }

  bool PopPacket(VCMPacket* packet, int index) {
    std::list<VCMPacket>::iterator it = GetPacketIterator(index);
    if (it == packets_.end())
      return false;
    if (packet)
      *packet = (*it);
    packets_.erase(it);
    return true;
  }

  bool GetPacket(VCMPacket* packet, int index) {
    std::list<VCMPacket>::iterator it = GetPacketIterator(index);
    if (it == packets_.end())
      return false;
    if (packet)
      *packet = (*it);
    return true;
  }

  bool NextPacket(VCMPacket* packet) {
    if (packets_.empty())
      return false;
    if (packet != NULL)
      *packet = packets_.front();
    packets_.pop_front();
    return true;
  }

  uint16_t NextSequenceNumber() const {
    if (packets_.empty())
      return sequence_number_;
    return packets_.front().seqNum;
  }

  int PacketsRemaining() const {
    return packets_.size();
  }

 private:
  std::list<VCMPacket>::iterator GetPacketIterator(int index) {
    std::list<VCMPacket>::iterator it = packets_.begin();
    for (int i = 0; i < index; ++i) {
      ++it;
      if (it == packets_.end()) break;
    }
    return it;
  }

  std::list<VCMPacket> packets_;
  uint16_t sequence_number_;
  uint32_t timestamp_;
  int64_t start_time_;
  uint8_t packet_buffer[kMaxPacketSize];

  DISALLOW_COPY_AND_ASSIGN(StreamGenerator);
};

class TestRunningJitterBuffer : public ::testing::Test {
 protected:
  enum { kDataBufferSize = 10 };

  virtual void SetUp() {
    clock_.reset(new SimulatedClock(0));
    max_nack_list_size_ = 250;
    oldest_packet_to_nack_ = 450;
    jitter_buffer_ = new VCMJitterBuffer(clock_.get(), -1, -1, true);
    stream_generator = new StreamGenerator(0, 0, clock_->TimeInMilliseconds());
    jitter_buffer_->Start();
    jitter_buffer_->SetNackSettings(max_nack_list_size_,
                                    oldest_packet_to_nack_);
    memset(data_buffer_, 0, kDataBufferSize);
  }

  virtual void TearDown() {
    jitter_buffer_->Stop();
    delete stream_generator;
    delete jitter_buffer_;
  }

  VCMFrameBufferEnum InsertPacketAndPop(int index) {
    VCMPacket packet;
    VCMEncodedFrame* frame;

    packet.dataPtr = data_buffer_;
    bool packet_available = stream_generator->PopPacket(&packet, index);
    EXPECT_TRUE(packet_available);
    if (!packet_available)
      return kStateError;  // Return here to avoid crashes below.
    EXPECT_EQ(VCM_OK, jitter_buffer_->GetFrame(packet, frame));
    return jitter_buffer_->InsertPacket(frame, packet);
  }

  VCMFrameBufferEnum InsertPacket(int index) {
    VCMPacket packet;
    VCMEncodedFrame* frame;

    packet.dataPtr = data_buffer_;
    bool packet_available = stream_generator->GetPacket(&packet, index);
    EXPECT_TRUE(packet_available);
    if (!packet_available)
      return kStateError;  // Return here to avoid crashes below.
    EXPECT_EQ(VCM_OK, jitter_buffer_->GetFrame(packet, frame));
    return jitter_buffer_->InsertPacket(frame, packet);
  }

  void InsertFrame(FrameType frame_type) {
    stream_generator->GenerateFrame(frame_type,
                                    (frame_type != kFrameEmpty) ? 1 : 0,
                                    (frame_type == kFrameEmpty) ? 1 : 0,
                                    clock_->TimeInMilliseconds());
    EXPECT_EQ(kFirstPacket, InsertPacketAndPop(0));
    clock_->AdvanceTimeMilliseconds(kDefaultFramePeriodMs);
  }

  void InsertFrames(int num_frames, FrameType frame_type) {
    for (int i = 0; i < num_frames; ++i) {
      InsertFrame(frame_type);
    }
  }

  void DropFrame(int num_packets) {
    stream_generator->GenerateFrame(kVideoFrameDelta, num_packets, 0,
                                    clock_->TimeInMilliseconds());
    clock_->AdvanceTimeMilliseconds(kDefaultFramePeriodMs);
  }

  bool DecodeCompleteFrame() {
    VCMEncodedFrame* frame = jitter_buffer_->GetCompleteFrameForDecoding(0);
    bool ret = (frame != NULL);
    jitter_buffer_->ReleaseFrame(frame);
    return ret;
  }

  bool DecodeFrame() {
    VCMEncodedFrame* frame = jitter_buffer_->GetFrameForDecoding();
    bool ret = (frame != NULL);
    jitter_buffer_->ReleaseFrame(frame);
    return ret;
  }

  VCMJitterBuffer* jitter_buffer_;
  StreamGenerator* stream_generator;
  scoped_ptr<SimulatedClock> clock_;
  size_t max_nack_list_size_;
  int oldest_packet_to_nack_;
  uint8_t data_buffer_[kDataBufferSize];
};

class TestJitterBufferNack : public TestRunningJitterBuffer {
 protected:
  virtual void SetUp() {
    TestRunningJitterBuffer::SetUp();
    jitter_buffer_->SetNackMode(kNackInfinite, -1, -1);
  }

  virtual void TearDown() {
    TestRunningJitterBuffer::TearDown();
  }
};

TEST_F(TestRunningJitterBuffer, TestFull) {
  // Insert a key frame and decode it.
  InsertFrame(kVideoFrameKey);
  EXPECT_TRUE(DecodeCompleteFrame());
  DropFrame(1);
  // Fill the jitter buffer.
  InsertFrames(kMaxNumberOfFrames, kVideoFrameDelta);
  // Make sure we can't decode these frames.
  EXPECT_FALSE(DecodeCompleteFrame());
  // This frame will make the jitter buffer recycle frames until a key frame.
  // Since none is found it will have to wait until the next key frame before
  // decoding.
  InsertFrame(kVideoFrameDelta);
  EXPECT_FALSE(DecodeCompleteFrame());
}

TEST_F(TestRunningJitterBuffer, TestEmptyPackets) {
  // Make sure a frame can get complete even though empty packets are missing.
  stream_generator->GenerateFrame(kVideoFrameKey, 3, 3,
                                  clock_->TimeInMilliseconds());
  EXPECT_EQ(kFirstPacket, InsertPacketAndPop(4));
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(4));
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  EXPECT_EQ(kCompleteSession, InsertPacketAndPop(0));
}

TEST_F(TestRunningJitterBuffer, JitterEstimateMode) {
  // Default value (should be in kLastEstimate mode).
  InsertFrame(kVideoFrameKey);
  InsertFrame(kVideoFrameDelta);
  EXPECT_GT(20u, jitter_buffer_->EstimatedJitterMs());
  // Set kMaxEstimate with a 2 seconds initial delay.
  jitter_buffer_->SetMaxJitterEstimate(2000u);
  EXPECT_EQ(2000u, jitter_buffer_->EstimatedJitterMs());
  InsertFrame(kVideoFrameDelta);
  EXPECT_EQ(2000u, jitter_buffer_->EstimatedJitterMs());
  // Jitter cannot decrease.
  InsertFrames(2, kVideoFrameDelta);
  uint32_t je1 = jitter_buffer_->EstimatedJitterMs();
  InsertFrames(2, kVideoFrameDelta);
  EXPECT_GE(je1, jitter_buffer_->EstimatedJitterMs());
}

TEST_F(TestRunningJitterBuffer, StatisticsTest) {
  uint32_t num_delta_frames = 0;
  uint32_t num_key_frames = 0;
  jitter_buffer_->FrameStatistics(&num_delta_frames, &num_key_frames);
  EXPECT_EQ(0u, num_delta_frames);
  EXPECT_EQ(0u, num_key_frames);

  uint32_t framerate = 0;
  uint32_t bitrate = 0;
  jitter_buffer_->IncomingRateStatistics(&framerate, &bitrate);
  EXPECT_EQ(0u, framerate);
  EXPECT_EQ(0u, bitrate);

  // Insert a couple of key and delta frames.
  InsertFrame(kVideoFrameKey);
  InsertFrame(kVideoFrameDelta);
  InsertFrame(kVideoFrameDelta);
  InsertFrame(kVideoFrameKey);
  InsertFrame(kVideoFrameDelta);
  // Decode some of them to make sure the statistics doesn't depend on if frames
  // are decoded or not.
  EXPECT_TRUE(DecodeCompleteFrame());
  EXPECT_TRUE(DecodeCompleteFrame());
  jitter_buffer_->FrameStatistics(&num_delta_frames, &num_key_frames);
  EXPECT_EQ(3u, num_delta_frames);
  EXPECT_EQ(2u, num_key_frames);

  // Insert 20 more frames to get estimates of bitrate and framerate over
  // 1 second.
  for (int i = 0; i < 20; ++i) {
    InsertFrame(kVideoFrameDelta);
  }
  jitter_buffer_->IncomingRateStatistics(&framerate, &bitrate);
  // TODO(holmer): The current implementation returns the average of the last
  // two framerate calculations, which is why it takes two calls to reach the
  // actual framerate. This should be fixed.
  EXPECT_EQ(kDefaultFrameRate / 2u, framerate);
  EXPECT_EQ(kDefaultBitrateKbps, bitrate);
  // Insert 25 more frames to get estimates of bitrate and framerate over
  // 2 seconds.
  for (int i = 0; i < 25; ++i) {
    InsertFrame(kVideoFrameDelta);
  }
  jitter_buffer_->IncomingRateStatistics(&framerate, &bitrate);
  EXPECT_EQ(kDefaultFrameRate, framerate);
  EXPECT_EQ(kDefaultBitrateKbps, bitrate);
}

TEST_F(TestJitterBufferNack, TestEmptyPackets) {
  // Make sure empty packets doesn't clog the jitter buffer.
  jitter_buffer_->SetNackMode(kNackHybrid, kLowRttNackMs, -1);
  InsertFrames(kMaxNumberOfFrames, kFrameEmpty);
  InsertFrame(kVideoFrameKey);
  EXPECT_TRUE(DecodeCompleteFrame());
}

TEST_F(TestJitterBufferNack, TestNackTooOldPackets) {
  // Insert a key frame and decode it.
  InsertFrame(kVideoFrameKey);
  EXPECT_TRUE(DecodeCompleteFrame());

  // Drop one frame and insert |kNackHistoryLength| to trigger NACKing a too
  // old packet.
  DropFrame(1);
  // Insert a frame which should trigger a recycle until the next key frame.
  InsertFrames(oldest_packet_to_nack_, kVideoFrameDelta);
  EXPECT_FALSE(DecodeCompleteFrame());

  uint16_t nack_list_length = max_nack_list_size_;
  bool extended;
  uint16_t* nack_list = jitter_buffer_->CreateNackList(&nack_list_length,
                                                       &extended);
  // Verify that the jitter buffer requests a key frame.
  EXPECT_TRUE(nack_list_length == 0xffff && nack_list == NULL);

  InsertFrame(kVideoFrameDelta);
  // Waiting for a key frame.
  EXPECT_FALSE(DecodeCompleteFrame());
  EXPECT_FALSE(DecodeFrame());

  InsertFrame(kVideoFrameKey);
  // The next complete continuous frame isn't a key frame, but we're waiting
  // for one.
  EXPECT_FALSE(DecodeCompleteFrame());
  // Skipping ahead to the key frame.
  EXPECT_TRUE(DecodeFrame());
}

TEST_F(TestJitterBufferNack, TestNackListFull) {
  // Insert a key frame and decode it.
  InsertFrame(kVideoFrameKey);
  EXPECT_TRUE(DecodeCompleteFrame());

  // Generate and drop |kNackHistoryLength| packets to fill the NACK list.
  DropFrame(max_nack_list_size_);
  // Insert a frame which should trigger a recycle until the next key frame.
  InsertFrame(kVideoFrameDelta);
  EXPECT_FALSE(DecodeCompleteFrame());

  uint16_t nack_list_length = max_nack_list_size_;
  bool extended;
  uint16_t* nack_list = jitter_buffer_->CreateNackList(&nack_list_length,
                                                       &extended);
  // Verify that the jitter buffer requests a key frame.
  EXPECT_TRUE(nack_list_length == 0xffff && nack_list == NULL);

  InsertFrame(kVideoFrameDelta);
  // Waiting for a key frame.
  EXPECT_FALSE(DecodeCompleteFrame());
  EXPECT_FALSE(DecodeFrame());

  InsertFrame(kVideoFrameKey);
  // The next complete continuous frame isn't a key frame, but we're waiting
  // for one.
  EXPECT_FALSE(DecodeCompleteFrame());
  // Skipping ahead to the key frame.
  EXPECT_TRUE(DecodeFrame());
}

TEST_F(TestJitterBufferNack, TestNackBeforeDecode) {
  DropFrame(10);
  // Insert a frame and try to generate a NACK list. Shouldn't get one.
  InsertFrame(kVideoFrameDelta);
  uint16_t nack_list_size = 0;
  bool extended = false;
  uint16_t* list = jitter_buffer_->CreateNackList(&nack_list_size, &extended);
  // No list generated, and a key frame request is signaled.
  EXPECT_TRUE(list == NULL);
  EXPECT_EQ(0xFFFF, nack_list_size);
}

TEST_F(TestJitterBufferNack, TestNormalOperation) {
  EXPECT_EQ(kNackInfinite, jitter_buffer_->nack_mode());

  InsertFrame(kVideoFrameKey);
  EXPECT_TRUE(DecodeFrame());

  //  ----------------------------------------------------------------
  // | 1 | 2 | .. | 8 | 9 | x | 11 | 12 | .. | 19 | x | 21 | .. | 100 |
  //  ----------------------------------------------------------------
  stream_generator->GenerateFrame(kVideoFrameKey, 100, 0,
                                  clock_->TimeInMilliseconds());
  clock_->AdvanceTimeMilliseconds(kDefaultFramePeriodMs);
  EXPECT_EQ(kFirstPacket, InsertPacketAndPop(0));
  // Verify that the frame is incomplete.
  EXPECT_FALSE(DecodeCompleteFrame());
  while (stream_generator->PacketsRemaining() > 1) {
    if (stream_generator->NextSequenceNumber() % 10 != 0)
      EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
    else
      stream_generator->NextPacket(NULL);  // Drop packet
  }
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  EXPECT_EQ(0, stream_generator->PacketsRemaining());
  EXPECT_FALSE(DecodeCompleteFrame());
  EXPECT_FALSE(DecodeFrame());
  uint16_t nack_list_size = 0;
  bool extended = false;
  uint16_t* list = jitter_buffer_->CreateNackList(&nack_list_size, &extended);
  // Verify the NACK list.
  const int kExpectedNackSize = 9;
  ASSERT_EQ(kExpectedNackSize, nack_list_size);
  for (int i = 0; i < nack_list_size; ++i)
    EXPECT_EQ((1 + i) * 10, list[i]);
}

TEST_F(TestJitterBufferNack, TestNormalOperationWrap) {
  //  -------   ------------------------------------------------------------
  // | 65532 | | 65533 | 65534 | 65535 | x | 1 | .. | 9 | x | 11 |.....| 96 |
  //  -------   ------------------------------------------------------------
  stream_generator->Init(65532, 0, clock_->TimeInMilliseconds());
  InsertFrame(kVideoFrameKey);
  EXPECT_TRUE(DecodeCompleteFrame());
  stream_generator->GenerateFrame(kVideoFrameDelta, 100, 0,
                                  clock_->TimeInMilliseconds());
  EXPECT_EQ(kFirstPacket, InsertPacketAndPop(0));
  while (stream_generator->PacketsRemaining() > 1) {
    if (stream_generator->NextSequenceNumber() % 10 != 0)
      EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
    else
      stream_generator->NextPacket(NULL);  // Drop packet
  }
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  EXPECT_EQ(0, stream_generator->PacketsRemaining());
  EXPECT_FALSE(DecodeCompleteFrame());
  EXPECT_FALSE(DecodeCompleteFrame());
  uint16_t nack_list_size = 0;
  bool extended = false;
  uint16_t* list = jitter_buffer_->CreateNackList(&nack_list_size, &extended);
  // Verify the NACK list.
  const int kExpectedNackSize = 10;
  ASSERT_EQ(kExpectedNackSize, nack_list_size);
  for (int i = 0; i < nack_list_size; ++i)
    EXPECT_EQ(i * 10, list[i]);
}

}  // namespace webrtc
