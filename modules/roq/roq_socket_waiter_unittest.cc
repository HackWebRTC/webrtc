/* Copyright (c) 2026 The WebRTC project authors. All Rights Reserved. */
#include "modules/roq/roq_socket_waiter.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <future>
#include <thread>

#include "test/gtest.h"

namespace webrtc::roq {
namespace {
using namespace std::chrono_literals;
using Clock = RoqSocketWaiter::Clock;

class RoqSocketWaiterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets_.data()), 0);
    ASSERT_EQ(waiter_.error(), 0);
    ASSERT_TRUE(waiter_.AttachSocket(sockets_[0]));
  }
  void TearDown() override {
    for (int fd : sockets_)
      if (fd >= 0) close(fd);
  }
  std::array<int, 2> sockets_ = {-1, -1};
  RoqSocketWaiter waiter_;
};

TEST_F(RoqSocketWaiterTest, QueuedNotificationSurvivesEnteringWait) {
  ASSERT_TRUE(waiter_.WakeUp());
  const auto event = waiter_.Wait(Clock::now() + 100ms);
  EXPECT_EQ(event.error, 0);
  EXPECT_TRUE(event.woken);
  EXPECT_FALSE(event.timed_out);
}

TEST_F(RoqSocketWaiterTest, WakeInterruptsAnIndefiniteWait) {
  std::promise<void> starting;
  auto result = std::async(std::launch::async, [&] {
    starting.set_value();
    return waiter_.Wait(std::nullopt);
  });
  starting.get_future().wait();
  ASSERT_TRUE(waiter_.WakeUp());
  const auto status = result.wait_for(1s);
  EXPECT_EQ(status, std::future_status::ready);
  if (status != std::future_status::ready) {
    // Let the waiter finish even if WakeUp itself is broken, so a regression
    // reports a test failure instead of hanging the test process.
    const char unblock = 0;
    send(sockets_[1], &unblock, 1, 0);
  }
  const auto event = result.get();
  EXPECT_TRUE(event.woken);
  EXPECT_EQ(event.error, 0);
}

TEST_F(RoqSocketWaiterTest, CoalescedNotificationsDoNotKeepIdleLoopAwake) {
  for (int i = 0; i < 1000; ++i) ASSERT_TRUE(waiter_.WakeUp());
  EXPECT_TRUE(waiter_.Wait(Clock::now() + 100ms).woken);
  const auto deadline = Clock::now() + 2ms;
  const auto event = waiter_.Wait(deadline);
  EXPECT_TRUE(event.timed_out);
  EXPECT_FALSE(event.woken);
  EXPECT_GE(Clock::now(), deadline);
}

TEST_F(RoqSocketWaiterTest, DeadlineDoesNotReturnBeforeItIsDue) {
  const auto deadline = Clock::now() + 350us;
  const auto event = waiter_.Wait(deadline);
  EXPECT_EQ(event.error, 0);
  EXPECT_TRUE(event.timed_out);
  EXPECT_GE(Clock::now(), deadline);
  EXPECT_FALSE(event.writable);
}

TEST_F(RoqSocketWaiterTest, ReadablePacketInterruptsLongDeadline) {
  const char packet[] = "rtp";
  ASSERT_EQ(send(sockets_[1], packet, sizeof(packet), 0), static_cast<ssize_t>(sizeof(packet)));
  const auto event = waiter_.Wait(Clock::now() + 1s);
  ASSERT_TRUE(event.readable);
  EXPECT_FALSE(event.timed_out);
  char received[sizeof(packet)] = {};
  EXPECT_EQ(recv(sockets_[0], received, sizeof(received), 0), static_cast<ssize_t>(sizeof(packet)));
  EXPECT_STREQ(received, "rtp");
  EXPECT_TRUE(waiter_.Wait(Clock::now() + 1ms).timed_out);
}

TEST_F(RoqSocketWaiterTest, WritableInterestIsEnabledOnlyWhenRequested) {
  EXPECT_TRUE(waiter_.Wait(Clock::now() + 1ms).timed_out);
  EXPECT_TRUE(waiter_.Wait(Clock::now() + 100ms, true).writable);
  const auto event = waiter_.Wait(Clock::now() + 1ms, false);
  EXPECT_TRUE(event.timed_out);
  EXPECT_FALSE(event.writable);
}

TEST_F(RoqSocketWaiterTest, InterruptedDeadlineDoesNotLeaveAStaleTimer) {
  ASSERT_TRUE(waiter_.WakeUp());
  EXPECT_TRUE(waiter_.Wait(Clock::now() + 1ms).woken);
  const auto later = Clock::now() + 8ms;
  EXPECT_TRUE(waiter_.Wait(later).timed_out);
  EXPECT_GE(Clock::now(), later);
}

TEST_F(RoqSocketWaiterTest, RepeatedProducerConsumerHandoffsDoNotLoseWakeups) {
  // Every notification is issued after the preceding notification was consumed,
  // but it may race the next Wait's setup and timer arming.
  for (int i = 0; i < 100; ++i) {
    auto producer = std::async(std::launch::async, [&] { return waiter_.WakeUp(); });
    const auto event = waiter_.Wait(Clock::now() + 100ms);
    EXPECT_TRUE(producer.get());
    ASSERT_TRUE(event.woken) << i;
    ASSERT_EQ(event.error, 0);
  }
}

TEST_F(RoqSocketWaiterTest, BackpressuredSocketWakesWhenPeerMakesSpace) {
  const int flags = fcntl(sockets_[0], F_GETFL);
  ASSERT_GE(flags, 0);
  ASSERT_EQ(fcntl(sockets_[0], F_SETFL, flags | O_NONBLOCK), 0);
  const int peer_flags = fcntl(sockets_[1], F_GETFL);
  ASSERT_GE(peer_flags, 0);
  ASSERT_EQ(fcntl(sockets_[1], F_SETFL, peer_flags | O_NONBLOCK), 0);
  std::array<char, 1024> packet = {};
  while (send(sockets_[0], packet.data(), packet.size(), 0) >= 0) {}
  ASSERT_TRUE(errno == EAGAIN || errno == EWOULDBLOCK) << errno;
  const auto blocked = waiter_.Wait(Clock::now() + 1ms, true);
  EXPECT_TRUE(blocked.timed_out);
  EXPECT_FALSE(blocked.writable);
  while (recv(sockets_[1], packet.data(), packet.size(), 0) >= 0) {}
  const auto ready = waiter_.Wait(Clock::now() + 100ms, true);
  EXPECT_TRUE(ready.writable);
  EXPECT_EQ(ready.error, 0);
}

TEST(RoqSocketWaiterInitializationTest, InvalidSocketIsRejected) {
  RoqSocketWaiter waiter;
  EXPECT_FALSE(waiter.AttachSocket(-1));
  EXPECT_NE(waiter.error(), 0);
}
}  // namespace
}  // namespace webrtc::roq
