/* Copyright (c) 2026 The WebRTC project authors. All Rights Reserved. */
#include "modules/roq/roq_socket_waiter.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstdint>

#if defined(__APPLE__)
#include <sys/event.h>
#elif defined(__linux__)
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#else
#error RoQ socket waiting requires Apple or Linux (including Android).
#endif

namespace webrtc::roq {
namespace {
timespec ToTimespec(std::chrono::nanoseconds duration) {
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
  return {static_cast<time_t>(seconds.count()),
          static_cast<long>((duration - seconds).count())};
}

#if defined(__APPLE__)
constexpr uintptr_t kWakeEvent = 1;
int Change(int fd, struct kevent* events, int count) {
  int result;
  do {
    result = kevent(fd, events, count, nullptr, 0, nullptr);
  } while (result < 0 && errno == EINTR);
  return result;
}
#else
bool AddDescriptor(int poll_fd, int fd, uint32_t events) {
  epoll_event event = {};
  event.events = events;
  event.data.fd = fd;
  return epoll_ctl(poll_fd, EPOLL_CTL_ADD, fd, &event) == 0;
}
void DrainCounter(int fd) {
  uint64_t value;
  // One read consumes all pending eventfd/timerfd counts. Do not keep draining:
  // a producer that signals after this read belongs to the next Wait.
  while (read(fd, &value, sizeof(value)) < 0 && errno == EINTR) {}
}
#endif
}  // namespace

RoqSocketWaiter::RoqSocketWaiter() {
#if defined(__APPLE__)
  poll_fd_ = kqueue();
  if (poll_fd_ < 0 || fcntl(poll_fd_, F_SETFD, FD_CLOEXEC) < 0) {
    error_ = errno;
    return;
  }
  struct kevent event;
  EV_SET(&event, kWakeEvent, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, nullptr);
  if (Change(poll_fd_, &event, 1) < 0) error_ = errno;
#else
  poll_fd_ = epoll_create1(EPOLL_CLOEXEC);
  wake_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  if (poll_fd_ < 0 || wake_fd_ < 0 || timer_fd_ < 0 ||
      !AddDescriptor(poll_fd_, wake_fd_, EPOLLIN) ||
      !AddDescriptor(poll_fd_, timer_fd_, EPOLLIN))
    error_ = errno;
#endif
}

RoqSocketWaiter::~RoqSocketWaiter() {
  if (timer_fd_ >= 0) close(timer_fd_);
  if (wake_fd_ >= 0) close(wake_fd_);
  if (poll_fd_ >= 0) close(poll_fd_);
}

bool RoqSocketWaiter::AttachSocket(int socket) {
  if (error_) return false;
  if (socket < 0 || socket_fd_ >= 0) {
    error_ = EINVAL;
    return false;
  }
#if defined(__APPLE__)
  std::array<struct kevent, 2> events;
  EV_SET(&events[0], socket, EVFILT_READ, EV_ADD, 0, 0, nullptr);
  EV_SET(&events[1], socket, EVFILT_WRITE, EV_ADD | EV_DISABLE, 0, 0, nullptr);
  if (Change(poll_fd_, events.data(), events.size()) < 0) {
#else
  if (!AddDescriptor(poll_fd_, socket, EPOLLIN)) {
#endif
    error_ = errno;
    return false;
  }
  socket_fd_ = socket;
  return true;
}

bool RoqSocketWaiter::WakeUp() {
  // poll_fd_/wake_fd_ are initialized before producers may access the session,
  // and remain alive until the owner has joined the network thread.
#if defined(__APPLE__)
  struct kevent event;
  EV_SET(&event, kWakeEvent, EVFILT_USER, 0, NOTE_TRIGGER, 0, nullptr);
  return Change(poll_fd_, &event, 1) == 0;
#else
  const uint64_t value = 1;
  ssize_t written;
  do {
    written = write(wake_fd_, &value, sizeof(value));
  } while (written < 0 && errno == EINTR);
  // A saturated counter already represents a persistent wakeup.
  return written == sizeof(value) || (written < 0 && errno == EAGAIN);
#endif
}

RoqSocketWaiter::Events RoqSocketWaiter::Wait(Deadline deadline,
                                             bool wait_for_write) {
  if (error_) return {.error = error_};
  if (socket_fd_ < 0) return {.error = EINVAL};
  if (wait_for_write != watch_write_) {
#if defined(__APPLE__)
    struct kevent event;
    EV_SET(&event, socket_fd_, EVFILT_WRITE,
           wait_for_write ? EV_ENABLE : EV_DISABLE, 0, 0, nullptr);
    if (Change(poll_fd_, &event, 1) < 0) return {.error = errno};
#else
    epoll_event event = {};
    event.events = EPOLLIN | (wait_for_write ? EPOLLOUT : 0u);
    event.data.fd = socket_fd_;
    if (epoll_ctl(poll_fd_, EPOLL_CTL_MOD, socket_fd_, &event) < 0)
      return {.error = errno};
#endif
    watch_write_ = wait_for_write;
  }

  for (;;) {
    const auto remaining = deadline
        ? std::max(*deadline - Clock::now(), Clock::duration::zero())
        : Clock::duration::zero();
#if defined(__APPLE__)
    const timespec timeout = ToTimespec(
        std::chrono::ceil<std::chrono::nanoseconds>(remaining));
    std::array<struct kevent, 3> events;
    const int count = kevent(poll_fd_, nullptr, 0, events.data(), events.size(),
                             deadline ? &timeout : nullptr);
#else
    // epoll_wait's timeout is millisecond based. A one-shot timerfd preserves
    // the full deadline precision, and can be interrupted by IO or producers.
    DrainCounter(timer_fd_);
    itimerspec timer = {};
    if (deadline && remaining > Clock::duration::zero())
      timer.it_value = ToTimespec(
          std::chrono::ceil<std::chrono::nanoseconds>(remaining));
    if (timerfd_settime(timer_fd_, 0, &timer, nullptr) < 0)
      return {.error = errno};
    std::array<epoll_event, 3> events;
    const int count = epoll_wait(poll_fd_, events.data(), events.size(),
                                deadline && remaining == Clock::duration::zero()
                                    ? 0 : -1);
#endif
    if (count < 0) {
      if (errno == EINTR) continue;
      return {.error = errno};
    }
    Events result;
    for (int i = 0; i < count; ++i) {
      const auto& event = events[i];
#if defined(__APPLE__)
      if (event.flags & EV_ERROR) result.error = static_cast<int>(event.data);
      if (event.filter == EVFILT_USER) result.woken = true;
      if (event.filter == EVFILT_READ) result.readable = true;
      if (event.filter == EVFILT_WRITE) result.writable = true;
#else
      if (event.data.fd == wake_fd_) {
        DrainCounter(wake_fd_);
        result.woken = true;
      } else if (event.data.fd == socket_fd_) {
        result.readable = event.events & (EPOLLIN | EPOLLERR | EPOLLHUP);
        result.writable = wait_for_write && (event.events & EPOLLOUT);
      } else if (event.data.fd == timer_fd_) {
        DrainCounter(timer_fd_);
      }
#endif
    }
    result.timed_out = deadline && Clock::now() >= *deadline;
    if (result.readable || result.writable || result.woken || result.error ||
        result.timed_out)
      return result;
    // Spurious/early timer or interrupted wait: preserve the original deadline.
  }
}
}  // namespace webrtc::roq
