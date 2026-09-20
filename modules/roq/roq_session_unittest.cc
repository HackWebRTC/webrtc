/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "api/roq/roq_session.h"

#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include <atomic>
#include <filesystem>
#include <future>
#include <thread>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "api/units/time_delta.h"
#include "test/gtest.h"
#include "rtc_tools/roq/test_roq_server.h"

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

class RoqSessionEventLoopTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(fd, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ASSERT_EQ(bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)), 0);
    socklen_t size = sizeof(address);
    ASSERT_EQ(getsockname(fd, reinterpret_cast<sockaddr*>(&address), &size), 0);
    const std::string endpoint = "127.0.0.1:" + std::to_string(ntohs(address.sin_port));
    close(fd);
    std::filesystem::path binary;
#if defined(__APPLE__)
    uint32_t path_size = 0;
    _NSGetExecutablePath(nullptr, &path_size);
    std::string executable(path_size, '\0');
    ASSERT_EQ(_NSGetExecutablePath(executable.data(), &path_size), 0);
    binary = std::filesystem::path(executable.c_str()).parent_path();
#else
    binary = std::filesystem::read_symlink("/proc/self/exe").parent_path();
#endif
    server_ = CreateTestRoqServer(
        {.address = endpoint,
         .certificate_path = (binary / "cert.crt").string(),
         .private_key_path = (binary / "cert.key").string(),
         .allowed_flow_ids = {8, 12}},
        [&](RoqReceivedPacket packet) {
          std::lock_guard lock(mutex_);
          received_.push_back(std::move(packet.packet));
          condition_.notify_all();
        }, {}, [&](std::vector<uint8_t> message) {
          std::lock_guard lock(mutex_);
          controls_.push_back(std::move(message));
          condition_.notify_all();
        });
    std::string error;
    ASSERT_TRUE(server_->Start(&error)) << error;
    ASSERT_TRUE(server_->WaitUntilReady(TimeDelta::Seconds(2)));
    client_ = RoqSession::Create(
        {.address = endpoint,
         .trusted_ca_file = (binary / "cert.crt").string(),
         .buffer_dynamic_receive_flows = true,
         .max_idle_timeout_ms = 20000,
         .allowed_flow_ids = {8}},
        [&](RoqReceivedPacket packet) {
          std::lock_guard lock(mutex_);
          downstream_.push_back(std::move(packet.packet));
          condition_.notify_all();
        });
    ASSERT_TRUE(client_->SendReplaySafeMessage(std::vector<uint8_t>{1}));
    ASSERT_TRUE(client_->Start(&error)) << error;
    ASSERT_TRUE(client_->WaitUntilEstablished(TimeDelta::Seconds(2))) << client_->error();
    std::unique_lock lock(mutex_);
    ASSERT_TRUE(condition_.wait_for(lock, std::chrono::seconds(2), [&] { return controls_.size() == 1; }));
  }
  void TearDown() override {
    if (client_) { client_->Abort(); client_.reset(); }
    if (server_) { server_->Abort(); server_.reset(); }
  }
  std::mutex mutex_;
  std::condition_variable condition_;
  std::vector<std::vector<uint8_t>> received_, controls_, downstream_;
  std::unique_ptr<TestRoqServer> server_;
  std::unique_ptr<RoqSession> client_;
};

TEST_F(RoqSessionEventLoopTest, IdleConnectionWakesForMediaAndControl) {
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  ASSERT_EQ(client_->SendMedia(8, std::vector<uint8_t>{7, 8, 9}, {}, {}), RoqSendResult::kQueued);
  ASSERT_TRUE(client_->SendReplaySafeMessage(std::vector<uint8_t>{2, 3}));
  std::unique_lock lock(mutex_);
  ASSERT_TRUE(condition_.wait_for(lock, std::chrono::milliseconds(200), [&] {
    return received_.size() == 1 && controls_.size() == 2;
  }));
  EXPECT_EQ(received_[0], (std::vector<uint8_t>{7, 8, 9}));
  EXPECT_EQ(controls_[1], (std::vector<uint8_t>{2, 3}));
}

TEST_F(RoqSessionEventLoopTest, FlowCommitDeliversBufferedMediaWithoutNewNetworkInput) {
  client_->BeginFlowUpdate();
  client_->PrepareReceiveFlowIds({8, 12});
  ASSERT_EQ(server_->SendMedia(12, std::vector<uint8_t>{42}, {}, {}), RoqSendResult::kQueued);
  const auto limit = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
  while (client_->stats().unknown_flow_buffered == 0 && std::chrono::steady_clock::now() < limit)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  ASSERT_GT(client_->stats().unknown_flow_buffered, 0u);
  client_->CommitFlowUpdate({{.flow_id = 8, .allow_send = true}, {.flow_id = 12}});
  std::unique_lock lock(mutex_);
  ASSERT_TRUE(condition_.wait_for(lock, std::chrono::milliseconds(50), [&] { return downstream_.size() == 1; }));
  EXPECT_EQ(downstream_[0], (std::vector<uint8_t>{42}));
}

TEST_F(RoqSessionEventLoopTest, MediaBurstLargerThanOneBatchIsNotStranded) {
  for (int i = 0; i < 130; ++i)
    ASSERT_EQ(client_->SendMedia(8, std::vector<uint8_t>{static_cast<uint8_t>(i)}, {}, {}), RoqSendResult::kQueued);
  std::unique_lock lock(mutex_);
  ASSERT_TRUE(condition_.wait_for(lock, std::chrono::seconds(2), [&] { return received_.size() == 130; }));
  for (int i = 0; i < 130; ++i)
    EXPECT_EQ(received_[i], std::vector<uint8_t>{static_cast<uint8_t>(i)});
}

TEST_F(RoqSessionEventLoopTest, AbortInterruptsIdleWait) {
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  client_->Abort();
  EXPECT_TRUE(client_->WaitUntilClosed(TimeDelta::Millis(100)));
}

TEST_F(RoqSessionEventLoopTest, PingAndCloseProgressAfterIdle) {
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  const auto sent_before = client_->stats().sent_udp_packets;
  client_->RequestPing();
  const auto limit = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
  while (client_->stats().sent_udp_packets <= sent_before && std::chrono::steady_clock::now() < limit)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  EXPECT_GT(client_->stats().sent_udp_packets, sent_before);
  client_->Close();
  EXPECT_TRUE(client_->WaitUntilClosed(TimeDelta::Millis(200)));
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
