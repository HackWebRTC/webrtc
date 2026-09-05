/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "api/roq/roq_session.h"

#include <errno.h>

#include <memory>
#include <string>

#include "api/units/time_delta.h"
#include "test/gtest.h"

namespace webrtc::roq {
namespace {

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
  EXPECT_EQ(ClassifyUdpSendResult(1200, 1200, 0),
            RoqUdpSendDisposition::kSent);
  EXPECT_EQ(ClassifyUdpSendResult(1000, 1200, 0),
            RoqUdpSendDisposition::kFatal);
  EXPECT_EQ(ClassifyUdpSendResult(-1, 1200, EBADF),
            RoqUdpSendDisposition::kFatal);
}

}  // namespace
}  // namespace webrtc::roq
