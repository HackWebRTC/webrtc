/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file includes unit tests for RemoteBitrateEstimator.

#include <gtest/gtest.h>

#include <algorithm>
#include <list>

#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "system_wrappers/interface/constructor_magic.h"
#include "system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

enum { kMtu = 1200 };

class TestBitrateObserver : public RemoteBitrateObserver {
 public:
  TestBitrateObserver() : updated_(false), latest_bitrate_(0) {}

  void OnReceiveBitrateChanged(unsigned int bitrate) {
    latest_bitrate_ = bitrate;
    updated_ = true;
  }

  void Reset() {
    updated_ = false;
  }

  bool updated() const {
    return updated_;
  }

  unsigned int latest_bitrate() const {
    return latest_bitrate_;
  }

 private:
  bool updated_;
  unsigned int latest_bitrate_;
};

class RtpStream {
 public:
  struct RtpPacket {
    int64_t send_time;
    int64_t arrival_time;
    uint32_t rtp_timestamp;
    unsigned int size;
    unsigned int ssrc;
  };

  struct RtcpPacket {
    uint32_t ntp_secs;
    uint32_t ntp_frac;
    uint32_t timestamp;
    unsigned int ssrc;
  };

  typedef std::list<RtpPacket*> PacketList;

  enum { kSendSideOffsetMs = 1000 };

  RtpStream(int fps, int bitrate_bps, unsigned int ssrc, unsigned int frequency,
            uint32_t timestamp_offset, int64_t rtcp_receive_time)
      : fps_(fps),
        bitrate_bps_(bitrate_bps),
        ssrc_(ssrc),
        frequency_(frequency),
        next_rtp_time_(0),
        next_rtcp_time_(rtcp_receive_time),
        rtp_timestamp_offset_(timestamp_offset),
        kNtpFracPerMs(4.294967296E6) {
    assert(fps_ > 0);
  }

  void set_rtp_timestamp_offset(uint32_t offset) {
    rtp_timestamp_offset_ = offset;
  }

  // Generates a new frame for this stream. If called too soon after the
  // previous frame, no frame will be generated. The frame is split into
  // packets.
  int64_t GenerateFrame(double time_now, PacketList* packets) {
    if (time_now < next_rtp_time_) {
      return next_rtp_time_;
    }
    assert(packets != NULL);
    int bits_per_frame = (bitrate_bps_ + fps_ / 2) / fps_;
    int n_packets = std::max((bits_per_frame + 8 * kMtu) / (8 * kMtu), 1);
    int packet_size = (bits_per_frame + 4 * n_packets) / (8 * n_packets);
    assert(n_packets >= 0);
    for (int i = 0; i < n_packets; ++i) {
      RtpPacket* packet = new RtpPacket;
      packet->send_time = time_now + kSendSideOffsetMs + 0.5f;
      packet->size = packet_size;
      packet->rtp_timestamp = rtp_timestamp_offset_ + static_cast<uint32_t>(
          (frequency_ / 1000.0) * packet->send_time + 0.5);
      packet->ssrc = ssrc_;
      packets->push_back(packet);
    }
    next_rtp_time_ = time_now + 1000.0 / static_cast<double>(fps_);
    return next_rtp_time_;
  }

  // The send-side time when the next frame can be generated.
  double next_rtp_time() const {
    return next_rtp_time_;
  }

  // Generates an RTCP packet.
  RtcpPacket* Rtcp(double time_now) {
    if (time_now < next_rtcp_time_) {
      return NULL;
    }
    RtcpPacket* rtcp = new RtcpPacket;
    int64_t send_time = RtpStream::kSendSideOffsetMs + time_now + 0.5;
    rtcp->timestamp = rtp_timestamp_offset_ + static_cast<uint32_t>(
        (frequency_ / 1000.0) * send_time + 0.5);
    rtcp->ntp_secs = send_time / 1000;
    rtcp->ntp_frac = (send_time % 1000) * kNtpFracPerMs;
    rtcp->ssrc = ssrc_;
    next_rtcp_time_ = time_now + kRtcpIntervalMs;
    return rtcp;
  }

  void set_bitrate_bps(int bitrate_bps) {
    ASSERT_GE(bitrate_bps, 0);
    bitrate_bps_ = bitrate_bps;
  }

  int bitrate_bps() const {
    return bitrate_bps_;
  }

  unsigned int ssrc() const {
    return ssrc_;
  }

  static bool Compare(const std::pair<unsigned int, RtpStream*>& left,
                      const std::pair<unsigned int, RtpStream*>& right) {
    return left.second->next_rtp_time_ < right.second->next_rtp_time_;
  }

 private:
  enum { kRtcpIntervalMs = 1000 };

  int fps_;
  int bitrate_bps_;
  unsigned int ssrc_;
  unsigned int frequency_;
  double next_rtp_time_;
  double next_rtcp_time_;
  uint32_t rtp_timestamp_offset_;
  const double kNtpFracPerMs;

  DISALLOW_COPY_AND_ASSIGN(RtpStream);
};

class StreamGenerator {
 public:
  typedef std::list<RtpStream::RtcpPacket*> RtcpList;

  StreamGenerator(int capacity, double time_now)
      : capacity_(capacity),
        prev_arrival_time_(time_now) {}

  ~StreamGenerator() {
    for (StreamMap::iterator it = streams_.begin(); it != streams_.end();
        ++it) {
      delete it->second;
    }
    streams_.clear();
  }

  // Add a new stream.
  void AddStream(RtpStream* stream) {
    streams_[stream->ssrc()] = stream;
  }

  // Set the link capacity.
  void set_capacity_bps(int capacity_bps) {
    ASSERT_GT(capacity_bps, 0);
    capacity_ = capacity_bps;
  }

  // Divides |bitrate_bps| among all streams. The allocated bitrate per stream
  // is decided by the initial allocation ratios.
  void set_bitrate_bps(int bitrate_bps) {
    ASSERT_GE(streams_.size(), 0u);
    double total_bitrate_before = 0;
    for (StreamMap::iterator it = streams_.begin(); it != streams_.end();
            ++it) {
      total_bitrate_before += it->second->bitrate_bps();
    }
    int total_bitrate_after = 0;
    for (StreamMap::iterator it = streams_.begin(); it != streams_.end();
        ++it) {
      double ratio = it->second->bitrate_bps() / total_bitrate_before;
      it->second->set_bitrate_bps(ratio * bitrate_bps + 0.5);
      total_bitrate_after += it->second->bitrate_bps();
    }
    EXPECT_NEAR(total_bitrate_after, bitrate_bps, 1);
  }

  // Set the RTP timestamp offset for the stream identified by |ssrc|.
  void set_rtp_timestamp_offset(unsigned int ssrc, uint32_t offset) {
    streams_[ssrc]->set_rtp_timestamp_offset(offset);
  }

  // TODO(holmer): Break out the channel simulation part from this class to make
  // it possible to simulate different types of channels.
  double GenerateFrame(RtpStream::PacketList* packets, double time_now) {
    assert(packets != NULL);
    assert(packets->empty());
    assert(capacity_ > 0);
    StreamMap::iterator it = std::min_element(streams_.begin(), streams_.end(),
                                              RtpStream::Compare);
    (*it).second->GenerateFrame(time_now, packets);
    for (RtpStream::PacketList::iterator packet_it = packets->begin();
        packet_it != packets->end(); ++packet_it) {
      int required_network_time =
          (8 * 1000 * (*packet_it)->size + capacity_ / 2) / capacity_;
      prev_arrival_time_ = std::max(time_now + required_network_time,
          prev_arrival_time_ + required_network_time);
      (*packet_it)->arrival_time = prev_arrival_time_ + 0.5;
    }
    it = std::min_element(streams_.begin(), streams_.end(), RtpStream::Compare);
    return (*it).second->next_rtp_time();
  }

  void Rtcps(RtcpList* rtcps, double time_now) const {
    for (StreamMap::const_iterator it = streams_.begin(); it != streams_.end();
        ++it) {
      RtpStream::RtcpPacket* rtcp = it->second->Rtcp(time_now);
      if (rtcp) {
        rtcps->push_front(rtcp);
      }
    }
  }

 private:
  typedef std::map<unsigned int, RtpStream*> StreamMap;

  // Capacity of the simulated channel in bits per second.
  int capacity_;
  // The time when the last packet arrived.
  double prev_arrival_time_;
  // All streams being transmitted on this simulated channel.
  StreamMap streams_;

  DISALLOW_COPY_AND_ASSIGN(StreamGenerator);
};

class RemoteBitrateEstimatorTest : public ::testing::Test {
 public:
  RemoteBitrateEstimatorTest()
      : time_now_(0.0),
        align_streams_(false) {}
  explicit RemoteBitrateEstimatorTest(bool align_streams)
      : time_now_(0.0),
        align_streams_(align_streams) {}

 protected:
  virtual void SetUp() {
    bitrate_observer_.reset(new TestBitrateObserver);
    bitrate_estimator_.reset(
        RemoteBitrateEstimator::Create(
            bitrate_observer_.get(),
            over_use_detector_options_,
            RemoteBitrateEstimator::kMultiStreamEstimation));
    stream_generator_.reset(new StreamGenerator(1e6,  // Capacity.
                                                time_now_));
  }

 void AddDefaultStream() {
   stream_generator_->AddStream(new RtpStream(
       30,          // Frames per second.
       3e5,         // Bitrate.
       1,           // SSRC.
       90000,       // RTP frequency.
       0xFFFFF000,  // Timestamp offset.
       0));         // RTCP receive time.
 }

  // Generates a frame of packets belonging to a stream at a given bitrate and
  // with a given ssrc. The stream is pushed through a very simple simulated
  // network, and is then given to the receive-side bandwidth estimator.
  // Returns true if an over-use was seen, false otherwise.
  // The StreamGenerator::updated() should be used to check for any changes in
  // target bitrate after the call to this function.
  bool GenerateAndProcessFrame(unsigned int ssrc, unsigned int bitrate_bps) {
    stream_generator_->set_bitrate_bps(bitrate_bps);
    RtpStream::PacketList packets;
    time_now_ = stream_generator_->GenerateFrame(&packets, time_now_);
    int64_t last_arrival_time = -1;
    bool prev_was_decrease = false;
    bool overuse = false;
    while (!packets.empty()) {
      RtpStream::RtpPacket* packet = packets.front();
      if (align_streams_) {
        StreamGenerator::RtcpList rtcps;
        stream_generator_->Rtcps(&rtcps, time_now_);
        for (StreamGenerator::RtcpList::iterator it = rtcps.begin();
            it != rtcps.end(); ++it) {
          bitrate_estimator_->IncomingRtcp((*it)->ssrc,
                                           (*it)->ntp_secs,
                                           (*it)->ntp_frac,
                                           (*it)->timestamp);
          delete *it;
        }
      }
      bitrate_observer_->Reset();
      bitrate_estimator_->IncomingPacket(packet->ssrc,
                                         packet->size,
                                         packet->arrival_time,
                                         packet->rtp_timestamp);
      if (bitrate_observer_->updated()) {
        // Verify that new estimates only are triggered by an overuse and a
        // rate decrease.
        overuse = true;
        EXPECT_LE(bitrate_observer_->latest_bitrate(), bitrate_bps);
        EXPECT_FALSE(prev_was_decrease);
        prev_was_decrease = true;
      } else {
        prev_was_decrease = false;
      }
      last_arrival_time = packet->arrival_time;
      delete packet;
      packets.pop_front();
    }
    EXPECT_GT(last_arrival_time, -1);
    bitrate_estimator_->UpdateEstimate(ssrc, last_arrival_time);
    return overuse;
  }

  // Run the bandwidth estimator with a stream of |number_of_frames| frames.
  // Can for instance be used to run the estimator for some time to get it
  // into a steady state.
  unsigned int SteadyStateRun(unsigned int ssrc,
                              int number_of_frames,
                              unsigned int start_bitrate,
                              unsigned int min_bitrate,
                              unsigned int max_bitrate) {
    unsigned int bitrate_bps = start_bitrate;
    bool bitrate_update_seen = false;
    // Produce |number_of_frames| frames and give them to the estimator.
    for (int i = 0; i < number_of_frames; ++i) {
      bool overuse = GenerateAndProcessFrame(ssrc, bitrate_bps);
      if (overuse) {
        EXPECT_LT(bitrate_observer_->latest_bitrate(), max_bitrate);
        EXPECT_GT(bitrate_observer_->latest_bitrate(), min_bitrate);
        bitrate_bps = bitrate_observer_->latest_bitrate();
        bitrate_update_seen = true;
      } else if (bitrate_observer_->updated()) {
        bitrate_bps = bitrate_observer_->latest_bitrate();
        bitrate_observer_->Reset();
      }
    }
    EXPECT_TRUE(bitrate_update_seen);
    return bitrate_bps;
  }

  static const unsigned int kDefaultSsrc = 1;

  double time_now_;  // Current time at the receiver.
  OverUseDetectorOptions over_use_detector_options_;
  scoped_ptr<RemoteBitrateEstimator> bitrate_estimator_;
  scoped_ptr<TestBitrateObserver> bitrate_observer_;
  scoped_ptr<StreamGenerator> stream_generator_;
  const bool align_streams_;

  DISALLOW_COPY_AND_ASSIGN(RemoteBitrateEstimatorTest);
};

class RemoteBitrateEstimatorTestAlign : public RemoteBitrateEstimatorTest {
 public:
  RemoteBitrateEstimatorTestAlign() : RemoteBitrateEstimatorTest(true) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteBitrateEstimatorTestAlign);
};

TEST_F(RemoteBitrateEstimatorTest, TestInitialBehavior) {
  unsigned int bitrate_bps = 0;
  int64_t time_now = 0;
  uint32_t timestamp = 0;
  EXPECT_FALSE(bitrate_estimator_->LatestEstimate(kDefaultSsrc, &bitrate_bps));
  bitrate_estimator_->UpdateEstimate(kDefaultSsrc, time_now);
  EXPECT_FALSE(bitrate_estimator_->LatestEstimate(kDefaultSsrc, &bitrate_bps));
  EXPECT_FALSE(bitrate_observer_->updated());
  bitrate_observer_->Reset();
  // Inserting a packet. Still no valid estimate. We need to wait 1 second.
  bitrate_estimator_->IncomingPacket(kDefaultSsrc, kMtu, time_now,
                                     timestamp);
  bitrate_estimator_->UpdateEstimate(kDefaultSsrc, time_now);
  EXPECT_FALSE(bitrate_estimator_->LatestEstimate(kDefaultSsrc, &bitrate_bps));
  EXPECT_FALSE(bitrate_observer_->updated());
  bitrate_observer_->Reset();
  // Waiting more than one second gives us a valid estimate.
  // We need at least two packets for the incoming bitrate to be > 0 since the
  // window is 500 ms.
  time_now += 499;
  bitrate_estimator_->IncomingPacket(kDefaultSsrc, kMtu, time_now,
                                     timestamp);
  time_now += 2;
  bitrate_estimator_->UpdateEstimate(kDefaultSsrc, time_now);
  EXPECT_TRUE(bitrate_estimator_->LatestEstimate(kDefaultSsrc, &bitrate_bps));
  EXPECT_EQ(20644u, bitrate_bps);
  EXPECT_TRUE(bitrate_observer_->updated());
  bitrate_observer_->Reset();
  EXPECT_EQ(bitrate_observer_->latest_bitrate(), bitrate_bps);
}

// Make sure we initially increase the bitrate as expected.
TEST_F(RemoteBitrateEstimatorTest, TestRateIncreaseRtpTimestamps) {
  const int kExpectedIterations = 276;
  unsigned int bitrate_bps = 30000;
  int iterations = 0;
  AddDefaultStream();
  // Feed the estimator with a stream of packets and verify that it reaches
  // 500 kbps at the expected time.
  while (bitrate_bps < 5e5) {
    bool overuse = GenerateAndProcessFrame(kDefaultSsrc, bitrate_bps);
    if (overuse) {
      EXPECT_GT(bitrate_observer_->latest_bitrate(), bitrate_bps);
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_observer_->Reset();
    } else if (bitrate_observer_->updated()) {
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_observer_->Reset();
    }
    ++iterations;
    ASSERT_LE(iterations, kExpectedIterations);
  }
  ASSERT_EQ(kExpectedIterations, iterations);
}

// Verify that the time it takes for the estimator to reduce the bitrate when
// the capacity is tightened stays the same.
TEST_F(RemoteBitrateEstimatorTest, TestCapacityDropRtpTimestamps) {
  const int kNumberOfFrames= 300;
  const int kStartBitrate = 900e3;
  const int kMinExpectedBitrate = 800e3;
  const int kMaxExpectedBitrate = 1100e3;
  AddDefaultStream();
  // Run in steady state to make the estimator converge.
  stream_generator_->set_capacity_bps(1000e3);
  unsigned int bitrate_bps = SteadyStateRun(kDefaultSsrc, kNumberOfFrames,
                                            kStartBitrate, kMinExpectedBitrate,
                                            kMaxExpectedBitrate);
  // Reduce the capacity and verify the decrease time.
  stream_generator_->set_capacity_bps(500e3);
  int64_t bitrate_drop_time = -1;
  for (int i = 0; i < 200; ++i) {
    GenerateAndProcessFrame(kDefaultSsrc, bitrate_bps);
    // Check for either increase or decrease.
    if (bitrate_observer_->updated()) {
      if (bitrate_drop_time == -1 &&
          bitrate_observer_->latest_bitrate() <= 500e3) {
        bitrate_drop_time = time_now_;
      }
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_observer_->Reset();
    }
  }
  EXPECT_EQ(10333, bitrate_drop_time);
}

// Verify that the time it takes for the estimator to reduce the bitrate when
// the capacity is tightened stays the same. This test also verifies that we
// handle wrap-arounds in this scenario.
TEST_F(RemoteBitrateEstimatorTest, TestCapacityDropRtpTimestampsWrap) {
  const int kFramerate= 30;
  const int kStartBitrate = 900e3;
  const int kMinExpectedBitrate = 800e3;
  const int kMaxExpectedBitrate = 1100e3;
  const int kSteadyStateTime = 8;  // Seconds.
  AddDefaultStream();
  // Trigger wrap right after the steady state run.
  stream_generator_->set_rtp_timestamp_offset(kDefaultSsrc,
      std::numeric_limits<uint32_t>::max() - kSteadyStateTime * 90000);
  // Run in steady state to make the estimator converge.
  stream_generator_->set_capacity_bps(1000e3);
  unsigned int bitrate_bps = SteadyStateRun(kDefaultSsrc,
                                            kSteadyStateTime * kFramerate,
                                            kStartBitrate,
                                            kMinExpectedBitrate,
                                            kMaxExpectedBitrate);
  bitrate_observer_->Reset();
  // Reduce the capacity and verify the decrease time.
  stream_generator_->set_capacity_bps(500e3);
  int64_t bitrate_drop_time = -1;
  for (int i = 0; i < 200; ++i) {
    GenerateAndProcessFrame(kDefaultSsrc, bitrate_bps);
    // Check for either increase or decrease.
    if (bitrate_observer_->updated()) {
      if (bitrate_drop_time == -1 &&
          bitrate_observer_->latest_bitrate() <= 500e3) {
        bitrate_drop_time = time_now_;
      }
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_observer_->Reset();
    }
  }
  EXPECT_EQ(8299, bitrate_drop_time);
}

// Verify that the time it takes for the estimator to reduce the bitrate when
// the capacity is tightened stays the same. This test also verifies that we
// handle wrap-arounds in this scenario. This test also converts the timestamps
// to NTP time.
TEST_F(RemoteBitrateEstimatorTestAlign, TestCapacityDropRtpTimestampsWrap) {
  const int kFramerate= 30;
  const int kStartBitrate = 900e3;
  const int kMinExpectedBitrate = 800e3;
  const int kMaxExpectedBitrate = 1100e3;
  const int kSteadyStateTime = 8;  // Seconds.
  AddDefaultStream();
  // Trigger wrap right after the steady state run.
  stream_generator_->set_rtp_timestamp_offset(kDefaultSsrc,
      std::numeric_limits<uint32_t>::max() - kSteadyStateTime * 90000);
  // Run in steady state to make the estimator converge.
  stream_generator_->set_capacity_bps(1000e3);
  unsigned int bitrate_bps = SteadyStateRun(kDefaultSsrc,
                                            kSteadyStateTime * kFramerate,
                                            kStartBitrate,
                                            kMinExpectedBitrate,
                                            kMaxExpectedBitrate);
  bitrate_observer_->Reset();
  // Reduce the capacity and verify the decrease time.
  stream_generator_->set_capacity_bps(500e3);
  int64_t bitrate_drop_time = -1;
  for (int i = 0; i < 200; ++i) {
    GenerateAndProcessFrame(kDefaultSsrc, bitrate_bps);
    // Check for either increase or decrease.
    if (bitrate_observer_->updated()) {
      if (bitrate_drop_time == -1 &&
          bitrate_observer_->latest_bitrate() <= 500e3) {
        bitrate_drop_time = time_now_;
      }
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_observer_->Reset();
    }
  }
  EXPECT_EQ(8299, bitrate_drop_time);
}

// Verify that the time it takes for the estimator to reduce the bitrate when
// the capacity is tightened stays the same. This test also verifies that we
// handle wrap-arounds in this scenario. This is a multi-stream test.
TEST_F(RemoteBitrateEstimatorTestAlign, TwoStreamsCapacityDropWithWrap) {
  const int kFramerate= 30;
  const int kStartBitrate = 900e3;
  const int kMinExpectedBitrate = 800e3;
  const int kMaxExpectedBitrate = 1100e3;
  const int kSteadyStateTime = 7;  // Seconds.
  stream_generator_->AddStream(new RtpStream(
      30,               // Frames per second.
      kStartBitrate/2,  // Bitrate.
      1,           // SSRC.
      90000,       // RTP frequency.
      0xFFFFF000,  // Timestamp offset.
      0));         // RTCP receive time.

  stream_generator_->AddStream(new RtpStream(
      15,               // Frames per second.
      kStartBitrate/2,  // Bitrate.
      2,           // SSRC.
      90000,       // RTP frequency.
      0x00000FFF,  // Timestamp offset.
      0));         // RTCP receive time.
  // Trigger wrap right after the steady state run.
  stream_generator_->set_rtp_timestamp_offset(kDefaultSsrc,
      std::numeric_limits<uint32_t>::max() - kSteadyStateTime * 90000);
  // Run in steady state to make the estimator converge.
  stream_generator_->set_capacity_bps(1000e3);
  unsigned int bitrate_bps = SteadyStateRun(kDefaultSsrc,
                                            kSteadyStateTime * kFramerate,
                                            kStartBitrate,
                                            kMinExpectedBitrate,
                                            kMaxExpectedBitrate);
  bitrate_observer_->Reset();
  // Reduce the capacity and verify the decrease time.
  stream_generator_->set_capacity_bps(500e3);
  int64_t bitrate_drop_time = -1;
  for (int i = 0; i < 200; ++i) {
    GenerateAndProcessFrame(kDefaultSsrc, bitrate_bps);
    // Check for either increase or decrease.
    if (bitrate_observer_->updated()) {
      if (bitrate_drop_time == -1 &&
          bitrate_observer_->latest_bitrate() <= 500e3) {
        bitrate_drop_time = time_now_;
      }
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_observer_->Reset();
    }
  }
  EXPECT_EQ(4966, bitrate_drop_time);
}

// Verify that the time it takes for the estimator to reduce the bitrate when
// the capacity is tightened stays the same. This test also verifies that we
// handle wrap-arounds in this scenario. This is a multi-stream test.
TEST_F(RemoteBitrateEstimatorTestAlign, ThreeStreams) {
  const int kFramerate= 30;
  const int kStartBitrate = 900e3;
  const int kMinExpectedBitrate = 800e3;
  const int kMaxExpectedBitrate = 1100e3;
  const int kSteadyStateTime = 11;  // Seconds.
  stream_generator_->AddStream(new RtpStream(
      30,           // Frames per second.
      kStartBitrate/2,  // Bitrate.
      1,            // SSRC.
      90000,        // RTP frequency.
      0xFFFFF000,   // Timestamp offset.
      0));          // RTCP receive time.

  stream_generator_->AddStream(new RtpStream(
      30,           // Frames per second.
      kStartBitrate/3,  // Bitrate.
      2,            // SSRC.
      90000,        // RTP frequency.
      0x00000FFF,   // Timestamp offset.
      0));          // RTCP receive time.

  stream_generator_->AddStream(new RtpStream(
      30,           // Frames per second.
      kStartBitrate/6,  // Bitrate.
      3,            // SSRC.
      90000,        // RTP frequency.
      0x00000FFF,   // Timestamp offset.
      0));          // RTCP receive time.
  // Trigger wrap right after the steady state run.
  stream_generator_->set_rtp_timestamp_offset(kDefaultSsrc,
      std::numeric_limits<uint32_t>::max() - kSteadyStateTime * 90000);
  // Run in steady state to make the estimator converge.
  stream_generator_->set_capacity_bps(1000e3);
  unsigned int bitrate_bps = SteadyStateRun(kDefaultSsrc,
                                            kSteadyStateTime * kFramerate,
                                            kStartBitrate,
                                            kMinExpectedBitrate,
                                            kMaxExpectedBitrate);
  bitrate_observer_->Reset();
  // Reduce the capacity and verify the decrease time.
  stream_generator_->set_capacity_bps(500e3);
  int64_t bitrate_drop_time = -1;
  for (int i = 0; i < 200; ++i) {
    GenerateAndProcessFrame(kDefaultSsrc, bitrate_bps);
    // Check for either increase or decrease.
    if (bitrate_observer_->updated()) {
      if (bitrate_drop_time == -1 &&
          bitrate_observer_->latest_bitrate() <= 500e3) {
        bitrate_drop_time = time_now_;
      }
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_observer_->Reset();
    }
  }
  EXPECT_EQ(3933, bitrate_drop_time);
}

}  // namespace webrtc
