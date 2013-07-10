/*
 * libjingle
 * Copyright 2005 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/base/common.h"
#include "talk/session/media/channelmanager.h"
#include "talk/session/media/mediamonitor.h"

namespace cricket {

enum {
  MSG_MONITOR_POLL = 1,
  MSG_MONITOR_START = 2,
  MSG_MONITOR_STOP = 3,
  MSG_MONITOR_SIGNAL = 4
};

MediaMonitor::MediaMonitor(talk_base::Thread* worker_thread,
                           talk_base::Thread* monitor_thread)
    : worker_thread_(worker_thread),
      monitor_thread_(monitor_thread), monitoring_(false), rate_(0) {
}

MediaMonitor::~MediaMonitor() {
  monitoring_ = false;
  monitor_thread_->Clear(this);
  worker_thread_->Clear(this);
}

void MediaMonitor::Start(uint32 milliseconds) {
  rate_ = milliseconds;
  if (rate_ < 100)
    rate_ = 100;
  worker_thread_->Post(this, MSG_MONITOR_START);
}

void MediaMonitor::Stop() {
  worker_thread_->Post(this, MSG_MONITOR_STOP);
  rate_ = 0;
}

void MediaMonitor::OnMessage(talk_base::Message* message) {
  talk_base::CritScope cs(&crit_);

  switch (message->message_id) {
  case MSG_MONITOR_START:
    ASSERT(talk_base::Thread::Current() == worker_thread_);
    if (!monitoring_) {
      monitoring_ = true;
      PollMediaChannel();
    }
    break;

  case MSG_MONITOR_STOP:
    ASSERT(talk_base::Thread::Current() == worker_thread_);
    if (monitoring_) {
      monitoring_ = false;
      worker_thread_->Clear(this);
    }
    break;

  case MSG_MONITOR_POLL:
    ASSERT(talk_base::Thread::Current() == worker_thread_);
    PollMediaChannel();
    break;

  case MSG_MONITOR_SIGNAL:
    ASSERT(talk_base::Thread::Current() == monitor_thread_);
    Update();
    break;
  }
}

void MediaMonitor::PollMediaChannel() {
  talk_base::CritScope cs(&crit_);
  ASSERT(talk_base::Thread::Current() == worker_thread_);

  GetStats();

  // Signal the monitoring thread, start another poll timer
  monitor_thread_->Post(this, MSG_MONITOR_SIGNAL);
  worker_thread_->PostDelayed(rate_, this, MSG_MONITOR_POLL);
}

}
