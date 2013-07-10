/*
 * libjingle
 * Copyright 2010, Google Inc.
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

#ifndef TALK_EXAMPLES_LOGIN_JINGLEINFOTASK_H_
#define TALK_EXAMPLES_LOGIN_JINGLEINFOTASK_H_

#include <vector>

#include "talk/p2p/client/httpportallocator.h"
#include "talk/xmpp/xmppengine.h"
#include "talk/xmpp/xmpptask.h"
#include "talk/base/sigslot.h"

namespace buzz {

class JingleInfoTask : public XmppTask {
 public:
  explicit JingleInfoTask(XmppTaskParentInterface* parent) :
    XmppTask(parent, XmppEngine::HL_TYPE) {}

  virtual int ProcessStart();
  void RefreshJingleInfoNow();

  sigslot::signal3<const std::string &,
                   const std::vector<std::string> &,
                   const std::vector<talk_base::SocketAddress> &>
                       SignalJingleInfo;

 protected:
  class JingleInfoGetTask;
  friend class JingleInfoGetTask;

  virtual bool HandleStanza(const XmlElement * stanza);
};
}

#endif  // TALK_EXAMPLES_LOGIN_JINGLEINFOTASK_H_
