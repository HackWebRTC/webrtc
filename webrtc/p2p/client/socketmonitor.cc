/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/client/socketmonitor.h"

#include "webrtc/base/common.h"

namespace cricket {

enum {
  MSG_MONITOR_POLL,
  MSG_MONITOR_START,
  MSG_MONITOR_STOP,
  MSG_MONITOR_SIGNAL
};

SocketMonitor::SocketMonitor(TransportChannel* channel,
                             rtc::Thread* worker_thread,
                             rtc::Thread* monitor_thread) {
  channel_ = channel;
  channel_thread_ = worker_thread;
  monitoring_thread_ = monitor_thread;
  monitoring_ = false;
}

SocketMonitor::~SocketMonitor() {
  channel_thread_->Clear(this);
  monitoring_thread_->Clear(this);
}

void SocketMonitor::Start(int milliseconds) {
  rate_ = milliseconds;
  if (rate_ < 250)
    rate_ = 250;
  channel_thread_->Post(this, MSG_MONITOR_START);
}

void SocketMonitor::Stop() {
  channel_thread_->Post(this, MSG_MONITOR_STOP);
}

void SocketMonitor::OnMessage(rtc::Message *message) {
  rtc::CritScope cs(&crit_);
  switch (message->message_id) {
    case MSG_MONITOR_START:
      ASSERT(rtc::Thread::Current() == channel_thread_);
      if (!monitoring_) {
        monitoring_ = true;
        PollSocket(true);
      }
      break;

    case MSG_MONITOR_STOP:
      ASSERT(rtc::Thread::Current() == channel_thread_);
      if (monitoring_) {
        monitoring_ = false;
        channel_thread_->Clear(this);
      }
      break;

    case MSG_MONITOR_POLL:
      ASSERT(rtc::Thread::Current() == channel_thread_);
      PollSocket(true);
      break;

    case MSG_MONITOR_SIGNAL: {
      ASSERT(rtc::Thread::Current() == monitoring_thread_);
      std::vector<ConnectionInfo> infos = connection_infos_;
      crit_.Leave();
      SignalUpdate(this, infos);
      crit_.Enter();
      break;
    }
  }
}

void SocketMonitor::PollSocket(bool poll) {
  ASSERT(rtc::Thread::Current() == channel_thread_);
  rtc::CritScope cs(&crit_);

  // Gather connection infos
  channel_->GetStats(&connection_infos_);

  // Signal the monitoring thread, start another poll timer
  monitoring_thread_->Post(this, MSG_MONITOR_SIGNAL);
  if (poll)
    channel_thread_->PostDelayed(rate_, this, MSG_MONITOR_POLL);
}

}  // namespace cricket
