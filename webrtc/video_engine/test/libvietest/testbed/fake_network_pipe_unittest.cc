/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/video_engine/test/libvietest/include/fake_network_pipe.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::Invoke;

namespace webrtc {

class MockReceiver : public PacketReceiver {
 public:
  MockReceiver() {}
  virtual ~MockReceiver() {}

  void IncomingPacket(uint8_t* data, int length) {
    IncomingData(data, length);
    delete [] data;
  }

  MOCK_METHOD2(IncomingData, void(uint8_t*, int));
};

class FakeNetworkPipeTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    TickTime::UseFakeClock(12345);
    receiver_.reset(new MockReceiver());
  }

  virtual void TearDown() {
  }

  void SendPackets(FakeNetworkPipe* pipe, int number_packets, int kPacketSize) {
    scoped_array<uint8_t> packet(new uint8_t[kPacketSize]);
    for (int i = 0; i < number_packets; ++i) {
      pipe->SendPacket(packet.get(), kPacketSize);
    }
  }

  int PacketTimeMs(int capacity_kbps, int kPacketSize) {
    return 8 * kPacketSize / capacity_kbps;
  }

  scoped_ptr<MockReceiver> receiver_;
};

void DeleteMemory(uint8_t* data, int length) { delete [] data; }

// Test the capacity link and verify we get as many packets as we expect.
TEST_F(FakeNetworkPipeTest, CapacityTest) {
  const int kQueueLength = 20;
  const int kNetworkDelayMs = 0;
  const int kLinkCapacityKbps = 80;
  const int kLossPercent = 0;
  scoped_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(receiver_.get(),
                                                       kQueueLength,
                                                       kNetworkDelayMs,
                                                       kLinkCapacityKbps,
                                                       kLossPercent));

  // Add 10 packets of 1000 bytes, = 80 kb, and verify it takes one second to
  // get through the pipe.
  const int kNumPackets = 10;
  const int kPacketSize = 1000;
  SendPackets(pipe.get(), kNumPackets , kPacketSize);

  // Time to get one packet through the link.
  const int kPacketTimeMs = PacketTimeMs(kLinkCapacityKbps, kPacketSize);

  // Time haven't increased yet, so we souldn't get any packets.
  EXPECT_CALL(*receiver_, IncomingData(_, _))
      .Times(0);
  pipe->NetworkProcess();

  // Advance enough time to release one packet.
  TickTime::AdvanceFakeClock(kPacketTimeMs);
  EXPECT_CALL(*receiver_, IncomingData(_, _))
      .Times(1);
  pipe->NetworkProcess();

  // Release all but one packet
  TickTime::AdvanceFakeClock(9 * kPacketTimeMs - 1);
  EXPECT_CALL(*receiver_, IncomingData(_, _))
      .Times(8);
  pipe->NetworkProcess();

  // And the last one.
  TickTime::AdvanceFakeClock(1);
  EXPECT_CALL(*receiver_, IncomingData(_, _))
      .Times(1);
  pipe->NetworkProcess();
}

// Test the extra network delay.
TEST_F(FakeNetworkPipeTest, ExtraDelayTest) {
  const int kQueueLength = 20;
  const int kNetworkDelayMs = 100;
  const int kLinkCapacityKbps = 80;
  const int kLossPercent = 0;
  scoped_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(receiver_.get(),
                                                       kQueueLength,
                                                       kNetworkDelayMs,
                                                       kLinkCapacityKbps,
                                                       kLossPercent));

  const int kNumPackets = 2;
  const int kPacketSize = 1000;
  SendPackets(pipe.get(), kNumPackets , kPacketSize);

  // Time to get one packet through the link.
  const int kPacketTimeMs = PacketTimeMs(kLinkCapacityKbps, kPacketSize);

  // Increase more than kPacketTimeMs, but not more than the extra delay.
  TickTime::AdvanceFakeClock(kPacketTimeMs);
  EXPECT_CALL(*receiver_, IncomingData(_, _))
      .Times(0);
  pipe->NetworkProcess();

  // Advance the network delay to get the first packet.
  TickTime::AdvanceFakeClock(kNetworkDelayMs);
  EXPECT_CALL(*receiver_, IncomingData(_, _))
      .Times(1);
  pipe->NetworkProcess();

  // Advance one more kPacketTimeMs to get the last packet.
  TickTime::AdvanceFakeClock(kPacketTimeMs);
  EXPECT_CALL(*receiver_, IncomingData(_, _))
      .Times(1);
  pipe->NetworkProcess();
}

// Test the number of buffers and packets are dropped when sending too many
// packets too quickly.
TEST_F(FakeNetworkPipeTest, QueueLengthTest) {
  const int kQueueLength = 2;
  const int kNetworkDelayMs = 0;
  const int kLinkCapacityKbps = 80;
  const int kLossPercent = 0;
  scoped_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(receiver_.get(),
                                                       kQueueLength,
                                                       kNetworkDelayMs,
                                                       kLinkCapacityKbps,
                                                       kLossPercent));

  const int kPacketSize = 1000;
  const int kPacketTimeMs = PacketTimeMs(kLinkCapacityKbps, kPacketSize);

  // Send three packets and verify only 2 are delivered.
  SendPackets(pipe.get(), 3, kPacketSize);

  // Increase time enough to deliver all three packets, verify only two are
  // delivered.
  TickTime::AdvanceFakeClock(3 * kPacketTimeMs);
  EXPECT_CALL(*receiver_, IncomingData(_, _))
      .Times(2);
  pipe->NetworkProcess();
}

// Test we get statistics as expected.
TEST_F(FakeNetworkPipeTest, StatisticsTest) {
  const int kQueueLength = 2;
  const int kNetworkDelayMs = 20;
  const int kLinkCapacityKbps = 80;
  const int kLossPercent = 0;
  scoped_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(receiver_.get(),
                                                       kQueueLength,
                                                       kNetworkDelayMs,
                                                       kLinkCapacityKbps,
                                                       kLossPercent));

  const int kPacketSize = 1000;
  const int kPacketTimeMs = PacketTimeMs(kLinkCapacityKbps, kPacketSize);

  // Send three packets and verify only 2 are delivered.
  SendPackets(pipe.get(), 3, kPacketSize);
  TickTime::AdvanceFakeClock(3 * kPacketTimeMs + kNetworkDelayMs);

  EXPECT_CALL(*receiver_, IncomingData(_, _))
      .Times(2);
  pipe->NetworkProcess();

  // Packet 1: kPacketTimeMs + kNetworkDelayMs, packet 2: 2 * kPacketTimeMs +
  // kNetworkDelayMs => 170 ms average.
  EXPECT_EQ(pipe->AverageDelay(), 170);
  EXPECT_EQ(pipe->sent_packets(), 2);
  EXPECT_EQ(pipe->dropped_packets(), 1);
  EXPECT_EQ(pipe->PercentageLoss(), 1/3.f);
}

}  // namespace webrtc
