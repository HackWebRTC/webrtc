/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/quic/quicconnectionhelper.h"

namespace cricket {

QuicAlarm* QuicConnectionHelper::CreateAlarm(
    net::QuicAlarm::Delegate* delegate) {
  return new QuicAlarm(GetClock(), thread_, delegate);
}

QuicAlarm::QuicAlarm(const net::QuicClock* clock,
                     rtc::Thread* thread,
                     QuicAlarm::Delegate* delegate)
    : net::QuicAlarm(delegate), clock_(clock), thread_(thread) {}

QuicAlarm::~QuicAlarm() {}

void QuicAlarm::OnMessage(rtc::Message* msg) {
  // The alarm may have been cancelled.
  if (!deadline().IsInitialized()) {
    return;
  }

  // The alarm may have been re-set to a later time.
  if (clock_->Now() < deadline()) {
    SetImpl();
    return;
  }

  Fire();
}

int64 QuicAlarm::GetDelay() const {
  return deadline().Subtract(clock_->Now()).ToMilliseconds();
}

void QuicAlarm::SetImpl() {
  DCHECK(deadline().IsInitialized());
  CancelImpl();  // Unregister if already posted.

  int64 delay_ms = GetDelay();
  if (delay_ms < 0) {
    delay_ms = 0;
  }
  thread_->PostDelayed(delay_ms, this);
}

void QuicAlarm::CancelImpl() {
  thread_->Clear(this);
}

QuicConnectionHelper::QuicConnectionHelper(rtc::Thread* thread)
    : thread_(thread) {}

QuicConnectionHelper::~QuicConnectionHelper() {}

const net::QuicClock* QuicConnectionHelper::GetClock() const {
  return &clock_;
}

net::QuicRandom* QuicConnectionHelper::GetRandomGenerator() {
  return net::QuicRandom::GetInstance();
}

}  // namespace cricket
