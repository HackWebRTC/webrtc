/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_CLIENT_SOCKETMONITOR_H_
#define WEBRTC_P2P_CLIENT_SOCKETMONITOR_H_

#include <vector>

#include "webrtc/p2p/base/transportchannel.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/thread.h"

namespace cricket {

class SocketMonitor : public rtc::MessageHandler,
                      public sigslot::has_slots<> {
 public:
  SocketMonitor(TransportChannel* channel,
                rtc::Thread* worker_thread,
                rtc::Thread* monitor_thread);
  ~SocketMonitor();

  void Start(int cms);
  void Stop();

  rtc::Thread* monitor_thread() { return monitoring_thread_; }

  sigslot::signal2<SocketMonitor*,
                   const std::vector<ConnectionInfo>&> SignalUpdate;

 protected:
  void OnMessage(rtc::Message* message);
  void PollSocket(bool poll);

  std::vector<ConnectionInfo> connection_infos_;
  TransportChannel* channel_;
  rtc::Thread* channel_thread_;
  rtc::Thread* monitoring_thread_;
  rtc::CriticalSection crit_;
  uint32 rate_;
  bool monitoring_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_CLIENT_SOCKETMONITOR_H_
