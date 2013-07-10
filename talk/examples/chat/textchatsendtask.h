/*
 * libjingle
 * Copyright 2004--2013, Google Inc.
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

#ifndef TALK_EXAMPLES_CHAT_TEXTCHATSENDTASK_H_
#define TALK_EXAMPLES_CHAT_TEXTCHATSENDTASK_H_

#include "talk/xmpp/xmpptask.h"

namespace buzz {

// A class to send chat messages to the XMPP server.
class TextChatSendTask : public XmppTask {
 public:
  // Arguments:
  //   parent a reference to task interface associated withe the XMPP client.
  explicit TextChatSendTask(XmppTaskParentInterface* parent);

  // Shuts down the thread associated with this task.
  virtual ~TextChatSendTask();

  // Forms the XMPP "chat" stanza with the specified receipient and message
  // and queues it up.
  XmppReturnStatus Send(const Jid& to, const std::string& message);

  // Picks up any "chat" stanzas from our queue and sends them to the server.
  virtual int ProcessStart();
};

}  // namespace buzz

#endif  // TALK_EXAMPLES_CHAT_TEXTCHATSENDTASK_H_

