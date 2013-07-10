/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
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

#ifndef TALK_P2P_CLIENT_SOCKETMONITOR_H_
#define TALK_P2P_CLIENT_SOCKETMONITOR_H_

#include <vector>

#include "talk/base/criticalsection.h"
#include "talk/base/sigslot.h"
#include "talk/base/thread.h"
#include "talk/p2p/base/transportchannel.h"

namespace cricket {

class SocketMonitor : public talk_base::MessageHandler,
                      public sigslot::has_slots<> {
 public:
  SocketMonitor(TransportChannel* channel,
                talk_base::Thread* worker_thread,
                talk_base::Thread* monitor_thread);
  ~SocketMonitor();

  void Start(int cms);
  void Stop();

  talk_base::Thread* monitor_thread() { return monitoring_thread_; }

  sigslot::signal2<SocketMonitor*,
                   const std::vector<ConnectionInfo>&> SignalUpdate;

 protected:
  void OnMessage(talk_base::Message* message);
  void PollSocket(bool poll);

  std::vector<ConnectionInfo> connection_infos_;
  TransportChannel* channel_;
  talk_base::Thread* channel_thread_;
  talk_base::Thread* monitoring_thread_;
  talk_base::CriticalSection crit_;
  uint32 rate_;
  bool monitoring_;
};

}  // namespace cricket

#endif  // TALK_P2P_CLIENT_SOCKETMONITOR_H_
