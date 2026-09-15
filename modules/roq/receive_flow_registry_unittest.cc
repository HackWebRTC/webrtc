/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "modules/roq/receive_flow_registry.h"

#include <cstdint>
#include <vector>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/gtest.h"

namespace webrtc::roq {
namespace {

TEST(ReceiveFlowRegistryTest, ReleasesBufferedPacketsInArrivalOrder) {
  ReceiveFlowRegistry registry({1, 2});
  registry.BeginUpdate(Timestamp::Millis(100));
  EXPECT_EQ(
      registry.Route({.flow_id = 7, .packet = {1}}, Timestamp::Millis(101)),
      ReceiveFlowRoute::kBuffered);
  EXPECT_EQ(
      registry.Route({.flow_id = 7, .packet = {2}}, Timestamp::Millis(102)),
      ReceiveFlowRoute::kBuffered);

  std::vector<RoqReceivedPacket> released = registry.Update({1, 2, 7});

  ASSERT_EQ(released.size(), 2u);
  EXPECT_EQ(released[0].packet, std::vector<uint8_t>({1}));
  EXPECT_EQ(released[1].packet, std::vector<uint8_t>({2}));
}

TEST(ReceiveFlowRegistryTest, DropsOldestAtPerFlowPacketLimit) {
  ReceiveFlowRegistry registry({1});
  registry.BeginUpdate(Timestamp::Millis(100));
  for (uint8_t value = 0; value < 33; ++value) {
    EXPECT_EQ(registry.Route({.flow_id = 7, .packet = {value}},
                             Timestamp::Millis(100)),
              ReceiveFlowRoute::kBuffered);
  }
  std::vector<RoqReceivedPacket> released = registry.Update({1, 7});
  ASSERT_EQ(released.size(), 24u);
  EXPECT_EQ(released.front().packet, std::vector<uint8_t>({9}));
  EXPECT_EQ(registry.dropped(), 9u);
}

TEST(ReceiveFlowRegistryTest, DropsOldestAtGlobalPacketLimit) {
  ReceiveFlowRegistry registry({1});
  registry.BeginUpdate(Timestamp::Millis(100));
  for (uint64_t flow_id = 10; flow_id < 43; ++flow_id) {
    EXPECT_EQ(registry.Route(
                  {.flow_id = flow_id, .packet = {static_cast<uint8_t>(flow_id)}},
                  Timestamp::Millis(100)),
              ReceiveFlowRoute::kBuffered);
  }

  std::vector<ReceiveFlowConfig> flows;
  for (uint64_t flow_id = 10; flow_id < 43; ++flow_id)
    flows.push_back({flow_id, ReceiveFlowPolicy::kDeliver});
  std::vector<RoqReceivedPacket> released = registry.Update(std::move(flows));

  ASSERT_EQ(released.size(), 32u);
  EXPECT_EQ(released.front().flow_id, 11u);
  EXPECT_EQ(registry.dropped(), 1u);
}

TEST(ReceiveFlowRegistryTest, DropsOldestAtGlobalByteLimit) {
  ReceiveFlowRegistry registry({1});
  registry.BeginUpdate(Timestamp::Millis(100));
  EXPECT_EQ(registry.Route({.flow_id = 7, .packet = std::vector<uint8_t>(40 * 1024)},
                           Timestamp::Millis(100)),
            ReceiveFlowRoute::kBuffered);
  EXPECT_EQ(registry.Route({.flow_id = 8, .packet = std::vector<uint8_t>(40 * 1024)},
                           Timestamp::Millis(100)),
            ReceiveFlowRoute::kBuffered);

  std::vector<RoqReceivedPacket> released = registry.Update(
      {{7, ReceiveFlowPolicy::kDeliver}, {8, ReceiveFlowPolicy::kDeliver}});

  ASSERT_EQ(released.size(), 1u);
  EXPECT_EQ(released.front().flow_id, 8u);
  EXPECT_EQ(registry.dropped(), 1u);
}

TEST(ReceiveFlowRegistryTest, GraceWindowExpiresAtOneHundredMilliseconds) {
  ReceiveFlowRegistry registry({1});
  registry.BeginUpdate(Timestamp::Millis(100));

  EXPECT_EQ(registry.Route({.flow_id = 7, .packet = {1}},
                           Timestamp::Millis(200)),
            ReceiveFlowRoute::kBuffered);
  EXPECT_EQ(registry.Route({.flow_id = 8, .packet = {2}},
                           Timestamp::Millis(201)),
            ReceiveFlowRoute::kDiscard);
  EXPECT_EQ(registry.dropped(), 2u);
}

TEST(ReceiveFlowRegistryTest, OneFlowCannotEvictEveryOtherFlow) {
  ReceiveFlowRegistry registry({1});
  registry.BeginUpdate(Timestamp::Millis(100));
  EXPECT_EQ(
      registry.Route({.flow_id = 8, .packet = {99}}, Timestamp::Millis(100)),
      ReceiveFlowRoute::kBuffered);
  for (uint8_t value = 0; value < 25; ++value) {
    EXPECT_EQ(
        registry.Route({.flow_id = 7, .packet = {value}}, Timestamp::Millis(100)),
        ReceiveFlowRoute::kBuffered);
  }

  std::vector<RoqReceivedPacket> released = registry.Update({1, 7, 8});

  ASSERT_EQ(released.size(), 25u);
  EXPECT_EQ(released.front().flow_id, 8u);
  EXPECT_EQ(released[1].packet, std::vector<uint8_t>({1}));
  EXPECT_EQ(registry.dropped(), 1u);
}

TEST(ReceiveFlowRegistryTest, DiscardsTombstonedFlowWithoutClosing) {
  ReceiveFlowRegistry registry({1});
  registry.Update({{1, ReceiveFlowPolicy::kDeliver},
                   {7, ReceiveFlowPolicy::kDiscard}});

  EXPECT_EQ(
      registry.Route({.flow_id = 7, .packet = {1}}, Timestamp::Millis(500)),
      ReceiveFlowRoute::kDiscard);
  EXPECT_EQ(registry.dropped(), 1u);
}

TEST(ReceiveFlowRegistryTest, ClosesOnlyAfterUnknownFlowAbuseThreshold) {
  ReceiveFlowRegistry registry({1});
  registry.BeginUpdate(Timestamp::Millis(100));
  EXPECT_EQ(
      registry.Route({.flow_id = 7, .packet = {1}}, Timestamp::Millis(201)),
      ReceiveFlowRoute::kDiscard);
  for (int i = 0; i < 30; ++i) {
    EXPECT_EQ(
        registry.Route({.flow_id = 7, .packet = {1}}, Timestamp::Millis(202 + i)),
        ReceiveFlowRoute::kDiscard);
  }
  EXPECT_EQ(
      registry.Route({.flow_id = 7, .packet = {1}}, Timestamp::Millis(250)),
      ReceiveFlowRoute::kClose);
  EXPECT_EQ(registry.dropped(), 32u);
}

TEST(ReceiveFlowRegistryTest, UnknownFlowAbuseWindowExpires) {
  ReceiveFlowRegistry registry({1});
  for (int i = 0; i < 31; ++i) {
    EXPECT_EQ(
        registry.Route({.flow_id = 7, .packet = {1}}, Timestamp::Millis(i)),
        ReceiveFlowRoute::kDiscard);
  }

  EXPECT_EQ(
      registry.Route({.flow_id = 7, .packet = {1}}, Timestamp::Millis(1001)),
      ReceiveFlowRoute::kDiscard);
}

TEST(ReceiveFlowRegistryTest, DynamicFlowWaitsForUnsolicitedMapping) {
  ReceiveFlowRegistry registry({1}, true);
  EXPECT_EQ(registry.Route({.flow_id = 7, .packet = {10}}, Timestamp::Millis(100)),
            ReceiveFlowRoute::kBuffered);
  EXPECT_EQ(registry.Route({.flow_id = 8, .packet = {20}}, Timestamp::Millis(101)),
            ReceiveFlowRoute::kBuffered);
  registry.BeginUpdate(Timestamp::Millis(105));
  // Installing the video flow first must not drop pending audio.
  auto first = registry.Update({1, 7}, Timestamp::Millis(110));
  ASSERT_EQ(first.size(), 1u);
  EXPECT_EQ(first[0].packet, std::vector<uint8_t>({10}));
  auto second = registry.Update({1, 7, 8}, Timestamp::Millis(115));
  ASSERT_EQ(second.size(), 1u);
  EXPECT_EQ(second[0].packet, std::vector<uint8_t>({20}));
}

TEST(ReceiveFlowRegistryTest, DynamicFlowGraceExpiresWithoutAnotherPacket) {
  ReceiveFlowRegistry registry({1}, true);
  ASSERT_EQ(registry.Route({.flow_id = 7, .packet = {10}}, Timestamp::Millis(100)),
            ReceiveFlowRoute::kBuffered);
  EXPECT_TRUE(registry.Update({1, 7}, Timestamp::Millis(201)).empty());
  EXPECT_EQ(registry.dropped(), 1u);
}

TEST(ReceiveFlowRegistryTest, DynamicFlowAbuseCannotExtendGraceForever) {
  ReceiveFlowRegistry registry({1}, true);
  // 24 retained packets plus 31 evicted unannounced packets.
  for (int i = 0; i < 55; ++i)
    EXPECT_EQ(registry.Route({.flow_id = 7, .packet = {10}}, Timestamp::Millis(100)),
              ReceiveFlowRoute::kBuffered);
  EXPECT_EQ(registry.Route({.flow_id = 7, .packet = {10}}, Timestamp::Millis(101)),
            ReceiveFlowRoute::kClose);
}

TEST(ReceiveFlowRegistryTest, RemovedAndUnsupportedDynamicFlowsStayDiscarded) {
  ReceiveFlowRegistry registry({1}, true);
  registry.Update({{2, ReceiveFlowPolicy::kDiscard}}, Timestamp::Millis(100));
  EXPECT_EQ(registry.Route({.flow_id = 1, .packet = {10}}, Timestamp::Millis(101)),
            ReceiveFlowRoute::kDiscard);
  EXPECT_EQ(registry.Route({.flow_id = 2, .packet = {10}}, Timestamp::Millis(102)),
            ReceiveFlowRoute::kDiscard);
  registry.Update({{3, ReceiveFlowPolicy::kDeliver}}, Timestamp::Millis(105));
  EXPECT_EQ(registry.Route({.flow_id = 2, .packet = {10}}, Timestamp::Millis(106)),
            ReceiveFlowRoute::kDiscard);
}

TEST(ReceiveFlowRegistryTest, DynamicFlowTrafficCannotExtendAnotherPacketsLifetime) {
  ReceiveFlowRegistry registry({1}, true);
  registry.Route({.flow_id = 7, .packet = {10}}, Timestamp::Millis(100));
  registry.Route({.flow_id = 8, .packet = {20}}, Timestamp::Millis(190));
  registry.BeginUpdate(Timestamp::Millis(199));
  auto released = registry.Update({1, 7, 8}, Timestamp::Millis(201));
  ASSERT_EQ(released.size(), 1u);
  EXPECT_EQ(released.front().flow_id, 8u);
  EXPECT_EQ(registry.dropped(), 1u);
}

TEST(ReceiveFlowRegistryTest, AnnouncedFlowDoesNotCountSlowMediaSetupAsAbuse) {
  ReceiveFlowRegistry registry({1}, true);
  registry.Prepare({7});
  for (int i = 0; i < 100; ++i) {
    ASSERT_EQ(registry.Route({.flow_id = 7, .packet = {static_cast<uint8_t>(i)}},
                            Timestamp::Millis(100 + i * 10)),
              ReceiveFlowRoute::kBuffered);
  }
  auto packets = registry.Update({1, 7}, Timestamp::Millis(1100));
  ASSERT_EQ(packets.size(), 10u);
  EXPECT_EQ(packets.front().packet, std::vector<uint8_t>({90}));
  EXPECT_EQ(packets.back().packet, std::vector<uint8_t>({99}));
  EXPECT_EQ(registry.dropped(), 90u);
}

}  // namespace
}  // namespace webrtc::roq
