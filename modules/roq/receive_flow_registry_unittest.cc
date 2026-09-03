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

TEST(ReceiveFlowRegistryTest, DropsOldestAtPacketLimit) {
  ReceiveFlowRegistry registry({1});
  registry.BeginUpdate(Timestamp::Millis(100));
  for (uint8_t value = 0; value < 33; ++value) {
    EXPECT_EQ(registry.Route({.flow_id = 7, .packet = {value}},
                             Timestamp::Millis(100)),
              ReceiveFlowRoute::kBuffered);
  }
  std::vector<RoqReceivedPacket> released = registry.Update({1, 7});
  ASSERT_EQ(released.size(), 32u);
  EXPECT_EQ(released.front().packet, std::vector<uint8_t>({1}));
  EXPECT_EQ(registry.dropped(), 1u);
}

TEST(ReceiveFlowRegistryTest, ClosesUnknownFlowAfterGraceWindow) {
  ReceiveFlowRegistry registry({1});
  registry.BeginUpdate(Timestamp::Millis(100));
  EXPECT_EQ(
      registry.Route({.flow_id = 7, .packet = {1}}, Timestamp::Millis(201)),
      ReceiveFlowRoute::kClose);
  EXPECT_EQ(registry.dropped(), 1u);
}

}  // namespace
}  // namespace webrtc::roq
