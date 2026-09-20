/* Copyright (c) 2026 The WebRTC project authors. All Rights Reserved. */
#ifndef MODULES_ROQ_ROQ_SOCKET_WAITER_H_
#define MODULES_ROQ_ROQ_SOCKET_WAITER_H_

#include <chrono>
#include <optional>

namespace webrtc::roq {

// One network-thread consumer. WakeUp may be called concurrently by producers;
// notifications persist across the transition from inspecting work to waiting.
// The attached socket is borrowed and must outlive all Wait calls.
class RoqSocketWaiter {
 public:
  using Clock = std::chrono::steady_clock;
  using Deadline = std::optional<Clock::time_point>;
  struct Events {
    bool readable = false;
    bool writable = false;
    bool woken = false;
    bool timed_out = false;
    int error = 0;
  };

  RoqSocketWaiter();
  ~RoqSocketWaiter();
  RoqSocketWaiter(const RoqSocketWaiter&) = delete;
  RoqSocketWaiter& operator=(const RoqSocketWaiter&) = delete;

  int error() const { return error_; }
  bool AttachSocket(int socket);
  bool WakeUp();
  Events Wait(Deadline deadline, bool wait_for_write = false);

 private:
  int poll_fd_ = -1;
  int wake_fd_ = -1;
  int timer_fd_ = -1;
  int socket_fd_ = -1;
  int error_ = 0;
  bool watch_write_ = false;
};

}  // namespace webrtc::roq
#endif  // MODULES_ROQ_ROQ_SOCKET_WAITER_H_
