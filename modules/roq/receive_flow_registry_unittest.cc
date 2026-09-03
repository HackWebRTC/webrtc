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

}  // namespace
}  // namespace webrtc::roq
