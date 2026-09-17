/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "api/roq/roq_session.h"

#include <errno.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "api/units/time_delta.h"
#include "test/gtest.h"

namespace webrtc::roq {
namespace {

TEST(RoqSessionTest, EarlyBudgetAppliesBeforeTransportStarts) {
  auto session = RoqSession::Create({.address = "localhost:443",
                                     .allow_early_datagrams = true,
                                     .allowed_flow_ids = {0, 64, 16384}},
                                    [](RoqReceivedPacket) {});
  ASSERT_TRUE(session);
  EXPECT_EQ(session->SendMedia(0, std::vector<uint8_t>(1000), {}, {}),
            RoqSendResult::kOversize);
  EXPECT_EQ(session->SendMedia(64, std::vector<uint8_t>(999), {}, {}),
            RoqSendResult::kOversize);
  EXPECT_EQ(session->SendMedia(16384, std::vector<uint8_t>(996), {}, {}),
            RoqSendResult::kQueued);
}

TEST(RoqSessionTest, PacketBudgetIncludesTheActualFlowVarint) {
  size_t packet_limit = 0;
  auto session = RoqSession::Create(
      {.address = "localhost:443", .allowed_flow_ids = {0, 64, 16384}},
      [](RoqReceivedPacket) {}, [&](size_t bytes) { packet_limit = bytes; });
  ASSERT_TRUE(session);
  session->UpdatePeerDatagramPayloadLimit(1000);
  EXPECT_EQ(packet_limit, 996u);
  EXPECT_EQ(
      session->SendMedia(16384, std::vector<uint8_t>(packet_limit), {}, {}),
      RoqSendResult::kQueued);
}

TEST(RoqSessionTest,
     FlowUpdateChangesThePacketBudgetWithoutRestartingTransport) {
  size_t packet_limit = 0;
  auto session = RoqSession::Create(
      {.address = "localhost:443", .allowed_flow_ids = {0}},
      [](RoqReceivedPacket) {}, [&](size_t bytes) { packet_limit = bytes; });
  session->UpdatePeerDatagramPayloadLimit(1000);
  ASSERT_EQ(packet_limit, 999u);
  session->CommitFlowUpdate({{.flow_id = 16384, .allow_send = true}});
  EXPECT_EQ(packet_limit, 996u);
}

TEST(RoqSessionTest, ReportsLowFrequencyLifecycleTransitions) {
  std::vector<RoqSessionLifecycleState> states;
  RoqSessionConfig config = {
      .address = "192.0.2.1:443",
      .lifecycle_callback =
          [&](RoqSessionLifecycleEvent event) {
            states.push_back(event.state);
          },
      .allowed_flow_ids = {0, 1, 2},
  };
  std::unique_ptr<RoqSession> session =
      RoqSession::Create(std::move(config), [](RoqReceivedPacket) {});
  ASSERT_TRUE(session);

  std::string error;
  ASSERT_TRUE(session->Start(&error)) << error;
  EXPECT_TRUE(session->WaitUntilReady(TimeDelta::Seconds(1)));
  session->Close();
  EXPECT_TRUE(session->WaitUntilClosed(TimeDelta::Seconds(1)));

  ASSERT_EQ(states.size(), 2u);
  EXPECT_EQ(states[0], RoqSessionLifecycleState::kReady);
  EXPECT_EQ(states[1], RoqSessionLifecycleState::kClosed);
}

TEST(RoqSessionTest, ReportsTerminalSetupFailureWithoutPolling) {
  std::vector<RoqSessionLifecycleEvent> events;
  std::mutex events_mutex;
  std::condition_variable events_condition;
  RoqSessionConfig config = {
      .address = "invalid-address",
      .lifecycle_callback =
          [&](RoqSessionLifecycleEvent event) {
            {
              std::lock_guard lock(events_mutex);
              events.push_back(std::move(event));
            }
            events_condition.notify_all();
          },
      .allowed_flow_ids = {0, 1, 2},
  };
  std::unique_ptr<RoqSession> session =
      RoqSession::Create(std::move(config), [](RoqReceivedPacket) {});
  ASSERT_TRUE(session);

  std::string error;
  ASSERT_TRUE(session->Start(&error));
  EXPECT_TRUE(session->WaitUntilClosed(TimeDelta::Seconds(1)));

  std::unique_lock lock(events_mutex);
  EXPECT_TRUE(events_condition.wait_for(lock, std::chrono::seconds(1),
                                        [&] { return !events.empty(); }));
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0].state, RoqSessionLifecycleState::kClosed);
  EXPECT_FALSE(events[0].error.empty());
}

TEST(RoqSessionTest, ClientBindsLocallyForRemoteServerAddress) {
  RoqSessionConfig config = {
      .address = "192.0.2.1:443",
      .allowed_flow_ids = {0, 1, 2},
  };
  std::unique_ptr<RoqSession> session =
      RoqSession::Create(std::move(config), [](RoqReceivedPacket) {});
  ASSERT_TRUE(session);

  std::string error;
  ASSERT_TRUE(session->Start(&error)) << error;
  EXPECT_TRUE(session->WaitUntilReady(TimeDelta::Seconds(1)))
      << session->error();

  session->Close();
  EXPECT_TRUE(session->WaitUntilClosed(TimeDelta::Seconds(1)));
}

TEST(RoqSessionTest, ClientResolvesHostname) {
  RoqSessionConfig config = {
      .address = "localhost:443",
      .allowed_flow_ids = {0, 1, 2},
  };
  std::unique_ptr<RoqSession> session =
      RoqSession::Create(std::move(config), [](RoqReceivedPacket) {});
  ASSERT_TRUE(session);

  std::string error;
  ASSERT_TRUE(session->Start(&error)) << error;
  EXPECT_TRUE(session->WaitUntilReady(TimeDelta::Seconds(1)))
      << session->error();

  session->Close();
  EXPECT_TRUE(session->WaitUntilClosed(TimeDelta::Seconds(1)));
}

TEST(RoqSessionTest, ClientAcceptsBracketedIpv6Address) {
  RoqSessionConfig config = {
      .address = "[::1]:443",
      .allowed_flow_ids = {0, 1, 2},
  };
  std::unique_ptr<RoqSession> session =
      RoqSession::Create(std::move(config), [](RoqReceivedPacket) {});
  ASSERT_TRUE(session);

  std::string error;
  ASSERT_TRUE(session->Start(&error)) << error;
  EXPECT_TRUE(session->WaitUntilReady(TimeDelta::Seconds(1)))
      << session->error();

  session->Close();
  EXPECT_TRUE(session->WaitUntilClosed(TimeDelta::Seconds(1)));
}

TEST(RoqSessionTest, ClientUsesFreshConnectionIdForEverySession) {
  auto create = [] {
    RoqSessionConfig config = {
        .address = "192.0.2.1:443",
        .allowed_flow_ids = {0, 1, 2},
    };
    return RoqSession::Create(std::move(config), [](RoqReceivedPacket) {});
  };
  std::unique_ptr<RoqSession> first = create();
  std::unique_ptr<RoqSession> second = create();
  std::string error;
  ASSERT_TRUE(first->Start(&error)) << error;
  ASSERT_TRUE(second->Start(&error)) << error;
  ASSERT_TRUE(first->WaitUntilReady(TimeDelta::Seconds(1)));
  ASSERT_TRUE(second->WaitUntilReady(TimeDelta::Seconds(1)));

  EXPECT_FALSE(first->stats().source_connection_id.empty());
  EXPECT_NE(first->stats().source_connection_id,
            second->stats().source_connection_id);

  first->Close();
  second->Close();
  EXPECT_TRUE(first->WaitUntilClosed(TimeDelta::Seconds(1)));
  EXPECT_TRUE(second->WaitUntilClosed(TimeDelta::Seconds(1)));
}

TEST(RoqSessionTest, RetriesOnlyTransientUdpSendFailures) {
  EXPECT_EQ(ClassifyUdpSendResult(-1, 1200, EAGAIN),
            RoqUdpSendDisposition::kRetry);
  EXPECT_EQ(ClassifyUdpSendResult(-1, 1200, EWOULDBLOCK),
            RoqUdpSendDisposition::kRetry);
  EXPECT_EQ(ClassifyUdpSendResult(-1, 1200, ENOBUFS),
            RoqUdpSendDisposition::kRetry);
  EXPECT_EQ(ClassifyUdpSendResult(-1, 1200, EINTR),
            RoqUdpSendDisposition::kRetry);
  EXPECT_EQ(ClassifyUdpSendResult(1200, 1200, 0), RoqUdpSendDisposition::kSent);
  EXPECT_EQ(ClassifyUdpSendResult(1000, 1200, 0),
            RoqUdpSendDisposition::kFatal);
  EXPECT_EQ(ClassifyUdpSendResult(-1, 1200, EBADF),
            RoqUdpSendDisposition::kFatal);
}

TEST(RoqSessionTest, ContinuesMediaBurstUntilPacingOrBackpressure) {
  EXPECT_TRUE(ShouldContinueDatagramBurst(1, 64, false, 0, 31));
  EXPECT_TRUE(ShouldContinueDatagramBurst(1, 64, false, 1, 31));
  EXPECT_TRUE(ShouldContinueDatagramBurst(1, 64, false, 1, 0));
  EXPECT_FALSE(ShouldContinueDatagramBurst(64, 64, false, 0, 31));
  EXPECT_FALSE(ShouldContinueDatagramBurst(1, 64, true, 0, 31));
  EXPECT_FALSE(ShouldContinueDatagramBurst(1, 64, false, 0, 0));
}

}  // namespace
}  // namespace webrtc::roq
