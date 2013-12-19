/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

const int kPacketSize1 = 100;
const int kPacketSize2 = 300;
const uint32_t kSsrc1 = 1;
const uint32_t kSsrc2 = 2;

class ReceiveStatisticsTest : public ::testing::Test {
 public:
  ReceiveStatisticsTest() :
      clock_(0),
      receive_statistics_(ReceiveStatistics::Create(&clock_)) {
    memset(&header1_, 0, sizeof(header1_));
    header1_.ssrc = kSsrc1;
    header1_.sequenceNumber = 0;
    memset(&header2_, 0, sizeof(header2_));
    header2_.ssrc = kSsrc2;
    header2_.sequenceNumber = 0;
  }

 protected:
  SimulatedClock clock_;
  scoped_ptr<ReceiveStatistics> receive_statistics_;
  RTPHeader header1_;
  RTPHeader header2_;
};

TEST_F(ReceiveStatisticsTest, TwoIncomingSsrcs) {
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  ++header1_.sequenceNumber;
  receive_statistics_->IncomingPacket(header2_, kPacketSize2, false);
  ++header2_.sequenceNumber;
  clock_.AdvanceTimeMilliseconds(100);
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  ++header1_.sequenceNumber;
  receive_statistics_->IncomingPacket(header2_, kPacketSize2, false);
  ++header2_.sequenceNumber;

  StreamStatistician* statistician =
      receive_statistics_->GetStatistician(kSsrc1);
  ASSERT_TRUE(statistician != NULL);
  EXPECT_GT(statistician->BitrateReceived(), 0u);
  uint32_t bytes_received = 0;
  uint32_t packets_received = 0;
  statistician->GetDataCounters(&bytes_received, &packets_received);
  EXPECT_EQ(200u, bytes_received);
  EXPECT_EQ(2u, packets_received);

  statistician =
      receive_statistics_->GetStatistician(kSsrc2);
  ASSERT_TRUE(statistician != NULL);
  EXPECT_GT(statistician->BitrateReceived(), 0u);
  statistician->GetDataCounters(&bytes_received, &packets_received);
  EXPECT_EQ(600u, bytes_received);
  EXPECT_EQ(2u, packets_received);

  StatisticianMap statisticians = receive_statistics_->GetActiveStatisticians();
  EXPECT_EQ(2u, statisticians.size());
  // Add more incoming packets and verify that they are registered in both
  // access methods.
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  ++header1_.sequenceNumber;
  receive_statistics_->IncomingPacket(header2_, kPacketSize2, false);
  ++header2_.sequenceNumber;

  statisticians[kSsrc1]->GetDataCounters(&bytes_received, &packets_received);
  EXPECT_EQ(300u, bytes_received);
  EXPECT_EQ(3u, packets_received);
  statisticians[kSsrc2]->GetDataCounters(&bytes_received, &packets_received);
  EXPECT_EQ(900u, bytes_received);
  EXPECT_EQ(3u, packets_received);

  receive_statistics_->GetStatistician(kSsrc1)->GetDataCounters(
      &bytes_received, &packets_received);
  EXPECT_EQ(300u, bytes_received);
  EXPECT_EQ(3u, packets_received);
  receive_statistics_->GetStatistician(kSsrc2)->GetDataCounters(
      &bytes_received, &packets_received);
  EXPECT_EQ(900u, bytes_received);
  EXPECT_EQ(3u, packets_received);
}

TEST_F(ReceiveStatisticsTest, ActiveStatisticians) {
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  ++header1_.sequenceNumber;
  clock_.AdvanceTimeMilliseconds(1000);
  receive_statistics_->IncomingPacket(header2_, kPacketSize2, false);
  ++header2_.sequenceNumber;
  StatisticianMap statisticians = receive_statistics_->GetActiveStatisticians();
  // Nothing should time out since only 1000 ms has passed since the first
  // packet came in.
  EXPECT_EQ(2u, statisticians.size());

  clock_.AdvanceTimeMilliseconds(7000);
  // kSsrc1 should have timed out.
  statisticians = receive_statistics_->GetActiveStatisticians();
  EXPECT_EQ(1u, statisticians.size());

  clock_.AdvanceTimeMilliseconds(1000);
  // kSsrc2 should have timed out.
  statisticians = receive_statistics_->GetActiveStatisticians();
  EXPECT_EQ(0u, statisticians.size());

  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  ++header1_.sequenceNumber;
  // kSsrc1 should be active again and the data counters should have survived.
  statisticians = receive_statistics_->GetActiveStatisticians();
  EXPECT_EQ(1u, statisticians.size());
  StreamStatistician* statistician =
      receive_statistics_->GetStatistician(kSsrc1);
  ASSERT_TRUE(statistician != NULL);
  uint32_t bytes_received = 0;
  uint32_t packets_received = 0;
  statistician->GetDataCounters(&bytes_received, &packets_received);
  EXPECT_EQ(200u, bytes_received);
  EXPECT_EQ(2u, packets_received);
}

TEST_F(ReceiveStatisticsTest, Callbacks) {
  class TestCallback : public RtcpStatisticsCallback {
   public:
    TestCallback()
        : RtcpStatisticsCallback(), num_calls_(0), ssrc_(0), stats_() {}
    virtual ~TestCallback() {}

    virtual void StatisticsUpdated(const RtcpStatistics& statistics,
                                   uint32_t ssrc) {
      ssrc_ = ssrc;
      stats_ = statistics;
      ++num_calls_;
    }

    uint32_t num_calls_;
    uint32_t ssrc_;
    RtcpStatistics stats_;
  } callback;

  receive_statistics_->RegisterRtcpStatisticsCallback(&callback);

  // Add some arbitrary data, with loss and jitter.
  header1_.sequenceNumber = 1;
  clock_.AdvanceTimeMilliseconds(7);
  header1_.timestamp += 3;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  header1_.sequenceNumber += 2;
  clock_.AdvanceTimeMilliseconds(9);
  header1_.timestamp += 9;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  --header1_.sequenceNumber;
  clock_.AdvanceTimeMilliseconds(13);
  header1_.timestamp += 47;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, true);
  header1_.sequenceNumber += 3;
  clock_.AdvanceTimeMilliseconds(11);
  header1_.timestamp += 17;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  ++header1_.sequenceNumber;

  EXPECT_EQ(0u, callback.num_calls_);

  // Call GetStatistics, simulating a timed rtcp sender thread.
  RtcpStatistics statistics;
  receive_statistics_->GetStatistician(kSsrc1)
      ->GetStatistics(&statistics, true);

  EXPECT_EQ(1u, callback.num_calls_);
  EXPECT_EQ(callback.ssrc_, kSsrc1);
  EXPECT_EQ(statistics.cumulative_lost, callback.stats_.cumulative_lost);
  EXPECT_EQ(statistics.extended_max_sequence_number,
            callback.stats_.extended_max_sequence_number);
  EXPECT_EQ(statistics.fraction_lost, callback.stats_.fraction_lost);
  EXPECT_EQ(statistics.jitter, callback.stats_.jitter);

  receive_statistics_->RegisterRtcpStatisticsCallback(NULL);

  // Add some more data.
  header1_.sequenceNumber = 1;
  clock_.AdvanceTimeMilliseconds(7);
  header1_.timestamp += 3;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  header1_.sequenceNumber += 2;
  clock_.AdvanceTimeMilliseconds(9);
  header1_.timestamp += 9;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  --header1_.sequenceNumber;
  clock_.AdvanceTimeMilliseconds(13);
  header1_.timestamp += 47;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, true);
  header1_.sequenceNumber += 3;
  clock_.AdvanceTimeMilliseconds(11);
  header1_.timestamp += 17;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  ++header1_.sequenceNumber;

  receive_statistics_->GetStatistician(kSsrc1)
      ->GetStatistics(&statistics, true);

  // Should not have been called after deregister.
  EXPECT_EQ(1u, callback.num_calls_);
}
}  // namespace webrtc
