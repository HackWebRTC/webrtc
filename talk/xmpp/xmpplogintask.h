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

#ifndef TALK_XMPP_LOGINTASK_H_
#define TALK_XMPP_LOGINTASK_H_

#include <string>
#include <vector>

#include "talk/base/logging.h"
#include "talk/base/scoped_ptr.h"
#include "talk/xmpp/jid.h"
#include "talk/xmpp/xmppengine.h"

namespace buzz {

class XmlElement;
class XmppEngineImpl;
class SaslMechanism;


// TODO: Rename to LoginTask.
class XmppLoginTask {

public:
  XmppLoginTask(XmppEngineImpl *pctx);
  ~XmppLoginTask();

  bool IsDone()
    { return state_ == LOGINSTATE_DONE; }
  void IncomingStanza(const XmlElement * element, bool isStart);
  void OutgoingStanza(const XmlElement *element);
  void set_allow_non_google_login(bool b)
    { allowNonGoogleLogin_ = b; }

private:
  enum LoginTaskState {
    LOGINSTATE_INIT = 0,
    LOGINSTATE_STREAMSTART_SENT,
    LOGINSTATE_STARTED_XMPP,
    LOGINSTATE_TLS_INIT,
    LOGINSTATE_AUTH_INIT,
    LOGINSTATE_BIND_INIT,
    LOGINSTATE_TLS_REQUESTED,
    LOGINSTATE_SASL_RUNNING,
    LOGINSTATE_BIND_REQUESTED,
    LOGINSTATE_SESSION_REQUESTED,
    LOGINSTATE_DONE,
  };

  const XmlElement * NextStanza();
  bool Advance();
  bool HandleStartStream(const XmlElement * element);
  bool HandleFeatures(const XmlElement * element);
  const XmlElement * GetFeature(const QName & name);
  bool Failure(XmppEngine::Error reason);
  void FlushQueuedStanzas();

  XmppEngineImpl * pctx_;
  bool authNeeded_;
  bool allowNonGoogleLogin_;
  LoginTaskState state_;
  const XmlElement * pelStanza_;
  bool isStart_;
  std::string iqId_;
  talk_base::scoped_ptr<XmlElement> pelFeatures_;
  Jid fullJid_;
  std::string streamId_;
  talk_base::scoped_ptr<std::vector<XmlElement *> > pvecQueuedStanzas_;

  talk_base::scoped_ptr<SaslMechanism> sasl_mech_;

#ifdef _DEBUG
  static const talk_base::ConstantLabel LOGINTASK_STATES[];
#endif  // _DEBUG
};

}

#endif  //  TALK_XMPP_LOGINTASK_H_
