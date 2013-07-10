/*
 * libjingle
 * Copyright 2011, Google Inc.
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

#ifndef TALK_XMPP_PINGTASK_H_
#define TALK_XMPP_PINGTASK_H_

#include "talk/base/messagehandler.h"
#include "talk/base/messagequeue.h"
#include "talk/xmpp/xmpptask.h"

namespace buzz {

// Task to periodically send pings to the server to ensure that the network
// connection is valid, implementing XEP-0199.
//
// This is especially useful on cellular networks because:
// 1. It keeps the connections alive through the cellular network's NATs or
//    proxies.
// 2. It detects when the server has crashed or any other case in which the
//    connection has broken without a fin or reset packet being sent to us.
class PingTask : public buzz::XmppTask, private talk_base::MessageHandler {
 public:
  PingTask(buzz::XmppTaskParentInterface* parent,
      talk_base::MessageQueue* message_queue, uint32 ping_period_millis,
      uint32 ping_timeout_millis);

  virtual bool HandleStanza(const buzz::XmlElement* stanza);
  virtual int ProcessStart();

  // Raised if there is no response to a ping within ping_timeout_millis.
  // The task is automatically aborted after a timeout.
  sigslot::signal0<> SignalTimeout;

 private:
  // Implementation of MessageHandler.
  virtual void OnMessage(talk_base::Message* msg);

  talk_base::MessageQueue* message_queue_;
  uint32 ping_period_millis_;
  uint32 ping_timeout_millis_;
  uint32 next_ping_time_;
  uint32 ping_response_deadline_; // 0 if the response has been received
};

} // namespace buzz

#endif  // TALK_XMPP_PINGTASK_H_
