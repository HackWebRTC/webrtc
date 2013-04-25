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

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/video_coding/main/source/jitter_buffer.h"
#include "webrtc/modules/video_coding/main/source/media_opt_util.h"
#include "webrtc/modules/video_coding/main/source/packet.h"
#include "webrtc/modules/video_coding/main/test/test_util.h"
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
    max_nack_list_size_ = 150;
    oldest_packet_to_nack_ = 250;
    jitter_buffer_ = new VCMJitterBuffer(clock_.get(), &event_factory_, -1, -1,
                                         true);
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

  VCMFrameBufferEnum InsertFrame(FrameType frame_type) {
    stream_generator->GenerateFrame(frame_type,
                                    (frame_type != kFrameEmpty) ? 1 : 0,
                                    (frame_type == kFrameEmpty) ? 1 : 0,
                                    clock_->TimeInMilliseconds());
    VCMFrameBufferEnum ret = InsertPacketAndPop(0);
    clock_->AdvanceTimeMilliseconds(kDefaultFramePeriodMs);
    return ret;
  }

  VCMFrameBufferEnum InsertFrames(int num_frames, FrameType frame_type) {
    VCMFrameBufferEnum ret_for_all = kNoError;
    for (int i = 0; i < num_frames; ++i) {
      VCMFrameBufferEnum ret = InsertFrame(frame_type);
      if (ret < kNoError) {
        ret_for_all = ret;
      } else if (ret_for_all >= kNoError) {
        ret_for_all = ret;
      }
    }
    return ret_for_all;
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

  bool DecodeIncompleteFrame() {
    VCMEncodedFrame* frame =
        jitter_buffer_->MaybeGetIncompleteFrameForDecoding();
    bool ret = (frame != NULL);
    jitter_buffer_->ReleaseFrame(frame);
    return ret;
  }

  VCMJitterBuffer* jitter_buffer_;
  StreamGenerator* stream_generator;
  scoped_ptr<SimulatedClock> clock_;
  NullEventFactory event_factory_;
  size_t max_nack_list_size_;
  int oldest_packet_to_nack_;
  uint8_t data_buffer_[kDataBufferSize];
};

class TestJitterBufferNack : public TestRunningJitterBuffer {
 protected:
  virtual void SetUp() {
    TestRunningJitterBuffer::SetUp();
    jitter_buffer_->SetNackMode(kNack, -1, -1);
  }

  virtual void TearDown() {
    TestRunningJitterBuffer::TearDown();
  }
};

TEST_F(TestRunningJitterBuffer, Full) {
  // Insert a key frame and decode it.
  EXPECT_GE(InsertFrame(kVideoFrameKey), kNoError);
  EXPECT_TRUE(DecodeCompleteFrame());
  DropFrame(1);
  // Fill the jitter buffer.
  EXPECT_GE(InsertFrames(kMaxNumberOfFrames, kVideoFrameDelta), kNoError);
  // Make sure we can't decode these frames.
  EXPECT_FALSE(DecodeCompleteFrame());
  // This frame will make the jitter buffer recycle frames until a key frame.
  // Since none is found it will have to wait until the next key frame before
  // decoding.
  EXPECT_GE(InsertFrame(kVideoFrameDelta), kNoError);
  EXPECT_FALSE(DecodeCompleteFrame());
}

TEST_F(TestRunningJitterBuffer, EmptyPackets) {
  // Make sure a frame can get complete even though empty packets are missing.
  stream_generator->GenerateFrame(kVideoFrameKey, 3, 3,
                                  clock_->TimeInMilliseconds());
  bool request_key_frame = false;
  EXPECT_EQ(kFirstPacket, InsertPacketAndPop(4));
  EXPECT_FALSE(request_key_frame);
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(4));
  EXPECT_FALSE(request_key_frame);
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  EXPECT_FALSE(request_key_frame);
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  EXPECT_FALSE(request_key_frame);
  EXPECT_EQ(kCompleteSession, InsertPacketAndPop(0));
  EXPECT_FALSE(request_key_frame);
}

TEST_F(TestRunningJitterBuffer, JitterEstimateMode) {
  bool request_key_frame = false;
  // Default value (should be in kLastEstimate mode).
  InsertFrame(kVideoFrameKey);
  EXPECT_FALSE(request_key_frame);
  InsertFrame(kVideoFrameDelta);
  EXPECT_FALSE(request_key_frame);
  EXPECT_GT(20u, jitter_buffer_->EstimatedJitterMs());
  // Set kMaxEstimate with a 2 seconds initial delay.
  jitter_buffer_->SetMaxJitterEstimate(2000u);
  EXPECT_EQ(2000u, jitter_buffer_->EstimatedJitterMs());
  InsertFrame(kVideoFrameDelta);
  EXPECT_FALSE(request_key_frame);
  EXPECT_EQ(2000u, jitter_buffer_->EstimatedJitterMs());
  // Jitter cannot decrease.
  InsertFrames(2, kVideoFrameDelta);
  EXPECT_FALSE(request_key_frame);
  uint32_t je1 = jitter_buffer_->EstimatedJitterMs();
  InsertFrames(2, kVideoFrameDelta);
  EXPECT_FALSE(request_key_frame);
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
  // Decode some of them to make sure the statistics doesn't depend on frames
  // being decoded.
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

TEST_F(TestRunningJitterBuffer, SkipToKeyFrame) {
  // Insert delta frames.
  EXPECT_GE(InsertFrames(5, kVideoFrameDelta), kNoError);
  // Can't decode without a key frame.
  EXPECT_FALSE(DecodeCompleteFrame());
  InsertFrame(kVideoFrameKey);
  // Skip to the next key frame.
  EXPECT_TRUE(DecodeCompleteFrame());
}

TEST_F(TestJitterBufferNack, EmptyPackets) {
  // Make sure empty packets doesn't clog the jitter buffer.
  jitter_buffer_->SetNackMode(kNack, media_optimization::kLowRttNackMs, -1);
  EXPECT_GE(InsertFrames(kMaxNumberOfFrames, kFrameEmpty), kNoError);
  InsertFrame(kVideoFrameKey);
  EXPECT_TRUE(DecodeCompleteFrame());
}

TEST_F(TestJitterBufferNack, NackTooOldPackets) {
  // Insert a key frame and decode it.
  EXPECT_GE(InsertFrame(kVideoFrameKey), kNoError);
  EXPECT_TRUE(DecodeCompleteFrame());

  // Drop one frame and insert |kNackHistoryLength| to trigger NACKing a too
  // old packet.
  DropFrame(1);
  // Insert a frame which should trigger a recycle until the next key frame.
  EXPECT_EQ(kFlushIndicator, InsertFrames(oldest_packet_to_nack_,
                                          kVideoFrameDelta));
  EXPECT_FALSE(DecodeCompleteFrame());

  uint16_t nack_list_length = max_nack_list_size_;
  bool request_key_frame = false;
  uint16_t* nack_list = jitter_buffer_->GetNackList(&nack_list_length,
                                                    &request_key_frame);
  // Verify that the jitter buffer requests a key frame.
  EXPECT_TRUE(request_key_frame);
  EXPECT_TRUE(nack_list == NULL);
  EXPECT_EQ(0, nack_list_length);

  EXPECT_GE(InsertFrame(kVideoFrameDelta), kNoError);
  // Waiting for a key frame.
  EXPECT_FALSE(DecodeCompleteFrame());
  EXPECT_FALSE(DecodeIncompleteFrame());

  // The next complete continuous frame isn't a key frame, but we're waiting
  // for one.
  EXPECT_FALSE(DecodeCompleteFrame());
  EXPECT_GE(InsertFrame(kVideoFrameKey), kNoError);
  // Skipping ahead to the key frame.
  EXPECT_TRUE(DecodeCompleteFrame());
}

TEST_F(TestJitterBufferNack, NackLargeJitterBuffer) {
  // Insert a key frame and decode it.
  EXPECT_GE(InsertFrame(kVideoFrameKey), kNoError);
  EXPECT_TRUE(DecodeCompleteFrame());

  // Insert a frame which should trigger a recycle until the next key frame.
  EXPECT_GE(InsertFrames(oldest_packet_to_nack_, kVideoFrameDelta), kNoError);

  uint16_t nack_list_length = max_nack_list_size_;
  bool request_key_frame = false;
  jitter_buffer_->GetNackList(&nack_list_length, &request_key_frame);
  // Verify that the jitter buffer does not request a key frame.
  EXPECT_FALSE(request_key_frame);
  // Verify that no packets are NACKed.
  EXPECT_EQ(0, nack_list_length);
  // Verify that we can decode the next frame.
  EXPECT_TRUE(DecodeCompleteFrame());
}

TEST_F(TestJitterBufferNack, NackListFull) {
  // Insert a key frame and decode it.
  EXPECT_GE(InsertFrame(kVideoFrameKey), kNoError);
  EXPECT_TRUE(DecodeCompleteFrame());

  // Generate and drop |kNackHistoryLength| packets to fill the NACK list.
  DropFrame(max_nack_list_size_);
  // Insert a frame which should trigger a recycle until the next key frame.
  EXPECT_EQ(kFlushIndicator, InsertFrame(kVideoFrameDelta));
  EXPECT_FALSE(DecodeCompleteFrame());

  uint16_t nack_list_length = max_nack_list_size_;
  bool request_key_frame = false;
  jitter_buffer_->GetNackList(&nack_list_length, &request_key_frame);
  // Verify that the jitter buffer requests a key frame.
  EXPECT_TRUE(request_key_frame);

  EXPECT_GE(InsertFrame(kVideoFrameDelta), kNoError);
  // The next complete continuous frame isn't a key frame, but we're waiting
  // for one.
  EXPECT_FALSE(DecodeCompleteFrame());
  EXPECT_FALSE(DecodeIncompleteFrame());
  EXPECT_GE(InsertFrame(kVideoFrameKey), kNoError);
  // Skipping ahead to the key frame.
  EXPECT_TRUE(DecodeCompleteFrame());
}

TEST_F(TestJitterBufferNack, NoNackListReturnedBeforeFirstDecode) {
  DropFrame(10);
  // Insert a frame and try to generate a NACK list. Shouldn't get one.
  EXPECT_GE(InsertFrame(kVideoFrameDelta), kNoError);
  uint16_t nack_list_size = 0;
  bool request_key_frame = false;
  uint16_t* list = jitter_buffer_->GetNackList(&nack_list_size,
                                                  &request_key_frame);
  // No list generated, and a key frame request is signaled.
  EXPECT_TRUE(list == NULL);
  EXPECT_EQ(0, nack_list_size);
  EXPECT_TRUE(request_key_frame);
}

TEST_F(TestJitterBufferNack, NackListBuiltBeforeFirstDecode) {
  stream_generator->Init(0, 0, clock_->TimeInMilliseconds());
  InsertFrame(kVideoFrameKey);
  stream_generator->GenerateFrame(kVideoFrameDelta, 2, 0,
                                  clock_->TimeInMilliseconds());
  stream_generator->NextPacket(NULL);  // Drop packet.
  EXPECT_EQ(kFirstPacket, InsertPacketAndPop(0));
  EXPECT_TRUE(DecodeCompleteFrame());
  uint16_t nack_list_size = 0;
  bool extended = false;
  uint16_t* list = jitter_buffer_->GetNackList(&nack_list_size, &extended);
  EXPECT_EQ(1, nack_list_size);
  EXPECT_TRUE(list != NULL);
}

TEST_F(TestJitterBufferNack, UseNackToRecoverFirstKeyFrame) {
  stream_generator->Init(0, 0, clock_->TimeInMilliseconds());
  stream_generator->GenerateFrame(kVideoFrameKey, 3, 0,
                                  clock_->TimeInMilliseconds());
  EXPECT_EQ(kFirstPacket, InsertPacketAndPop(0));
  // Drop second packet.
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(1));
  EXPECT_FALSE(DecodeCompleteFrame());
  uint16_t nack_list_size = 0;
  bool extended = false;
  uint16_t* list = jitter_buffer_->GetNackList(&nack_list_size, &extended);
  EXPECT_EQ(1, nack_list_size);
  ASSERT_TRUE(list != NULL);
  VCMPacket packet;
  stream_generator->GetPacket(&packet, 0);
  EXPECT_EQ(packet.seqNum, list[0]);
}

TEST_F(TestJitterBufferNack, NormalOperation) {
  EXPECT_EQ(kNack, jitter_buffer_->nack_mode());
  jitter_buffer_->DecodeWithErrors(true);

  EXPECT_GE(InsertFrame(kVideoFrameKey), kNoError);
  EXPECT_TRUE(DecodeIncompleteFrame());

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
    if (stream_generator->NextSequenceNumber() % 10 != 0) {
      EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
    } else {
      stream_generator->NextPacket(NULL);  // Drop packet
    }
  }
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  EXPECT_EQ(0, stream_generator->PacketsRemaining());
  EXPECT_FALSE(DecodeCompleteFrame());
  EXPECT_FALSE(DecodeIncompleteFrame());
  uint16_t nack_list_size = 0;
  bool request_key_frame = false;
  uint16_t* list = jitter_buffer_->GetNackList(&nack_list_size,
                                               &request_key_frame);
  // Verify the NACK list.
  const int kExpectedNackSize = 9;
  ASSERT_EQ(kExpectedNackSize, nack_list_size);
  for (int i = 0; i < nack_list_size; ++i)
    EXPECT_EQ((1 + i) * 10, list[i]);
}

TEST_F(TestJitterBufferNack, NormalOperationWrap) {
  bool request_key_frame = false;
  //  -------   ------------------------------------------------------------
  // | 65532 | | 65533 | 65534 | 65535 | x | 1 | .. | 9 | x | 11 |.....| 96 |
  //  -------   ------------------------------------------------------------
  stream_generator->Init(65532, 0, clock_->TimeInMilliseconds());
  InsertFrame(kVideoFrameKey);
  EXPECT_FALSE(request_key_frame);
  EXPECT_TRUE(DecodeCompleteFrame());
  stream_generator->GenerateFrame(kVideoFrameDelta, 100, 0,
                                  clock_->TimeInMilliseconds());
  EXPECT_EQ(kFirstPacket, InsertPacketAndPop(0));
  while (stream_generator->PacketsRemaining() > 1) {
    if (stream_generator->NextSequenceNumber() % 10 != 0) {
      EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
      EXPECT_FALSE(request_key_frame);
    } else {
      stream_generator->NextPacket(NULL);  // Drop packet.
    }
  }
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  EXPECT_FALSE(request_key_frame);
  EXPECT_EQ(0, stream_generator->PacketsRemaining());
  EXPECT_FALSE(DecodeCompleteFrame());
  EXPECT_FALSE(DecodeCompleteFrame());
  uint16_t nack_list_size = 0;
  bool extended = false;
  uint16_t* list = jitter_buffer_->GetNackList(&nack_list_size, &extended);
  // Verify the NACK list.
  const int kExpectedNackSize = 10;
  ASSERT_EQ(kExpectedNackSize, nack_list_size);
  for (int i = 0; i < nack_list_size; ++i)
    EXPECT_EQ(i * 10, list[i]);
}

}  // namespace webrtc
