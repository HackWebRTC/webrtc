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

#ifndef TALK_XMPP_XMPPCLIENT_H_
#define TALK_XMPP_XMPPCLIENT_H_

#include <string>
#include "talk/base/basicdefs.h"
#include "talk/base/sigslot.h"
#include "talk/base/task.h"
#include "talk/xmpp/asyncsocket.h"
#include "talk/xmpp/xmppclientsettings.h"
#include "talk/xmpp/xmppengine.h"
#include "talk/xmpp/xmpptask.h"

namespace buzz {

class PreXmppAuth;
class CaptchaChallenge;

// Just some non-colliding number.  Could have picked "1".
#define XMPP_CLIENT_TASK_CODE 0x366c1e47

/////////////////////////////////////////////////////////////////////
//
// XMPPCLIENT
//
/////////////////////////////////////////////////////////////////////
//
// See Task first.  XmppClient is a parent task for XmppTasks.
//
// XmppClient is a task which is designed to be the parent task for
// all tasks that depend on a single Xmpp connection.  If you want to,
// for example, listen for subscription requests forever, then your
// listener should be a task that is a child of the XmppClient that owns
// the connection you are using.  XmppClient has all the utility methods
// that basically drill through to XmppEngine.
//
// XmppClient is just a wrapper for XmppEngine, and if I were writing it
// all over again, I would make XmppClient == XmppEngine.  Why?
// XmppEngine needs tasks too, for example it has an XmppLoginTask which
// should just be the same kind of Task instead of an XmppEngine specific
// thing.  It would help do certain things like GAIA auth cleaner.
//
/////////////////////////////////////////////////////////////////////

class XmppClient : public XmppTaskParentInterface,
                   public XmppClientInterface,
                   public sigslot::has_slots<>
{
public:
  explicit XmppClient(talk_base::TaskParent * parent);
  virtual ~XmppClient();

  XmppReturnStatus Connect(const XmppClientSettings & settings,
                           const std::string & lang,
                           AsyncSocket * socket,
                           PreXmppAuth * preauth);

  virtual int ProcessStart();
  virtual int ProcessResponse();
  XmppReturnStatus Disconnect();

  sigslot::signal1<XmppEngine::State> SignalStateChange;
  XmppEngine::Error GetError(int *subcode);

  // When there is a <stream:error> stanza, return the stanza
  // so that they can be handled.
  const XmlElement *GetStreamError();

  // When there is an authentication error, we may have captcha info
  // that the user can use to unlock their account
  CaptchaChallenge GetCaptchaChallenge();

  // When authentication is successful, this returns the service token
  // (if we used GAIA authentication)
  std::string GetAuthMechanism();
  std::string GetAuthToken();

  XmppReturnStatus SendRaw(const std::string & text);

  XmppEngine* engine();

  sigslot::signal2<const char *, int> SignalLogInput;
  sigslot::signal2<const char *, int> SignalLogOutput;

  // As XmppTaskParentIntreface
  virtual XmppClientInterface* GetClient() { return this; }

  // As XmppClientInterface
  virtual XmppEngine::State GetState() const;
  virtual const Jid& jid() const;
  virtual std::string NextId();
  virtual XmppReturnStatus SendStanza(const XmlElement *stanza);
  virtual XmppReturnStatus SendStanzaError(const XmlElement * pelOriginal,
                                           XmppStanzaError code,
                                           const std::string & text);
  virtual void AddXmppTask(XmppTask *, XmppEngine::HandlerLevel);
  virtual void RemoveXmppTask(XmppTask *);

 private:
  friend class XmppTask;

  void OnAuthDone();

  // Internal state management
  enum {
    STATE_PRE_XMPP_LOGIN = STATE_NEXT,
    STATE_START_XMPP_LOGIN = STATE_NEXT + 1,
  };
  int Process(int state) {
    switch (state) {
      case STATE_PRE_XMPP_LOGIN: return ProcessTokenLogin();
      case STATE_START_XMPP_LOGIN: return ProcessStartXmppLogin();
      default: return Task::Process(state);
    }
  }

  std::string GetStateName(int state) const {
    switch (state) {
      case STATE_PRE_XMPP_LOGIN:      return "PRE_XMPP_LOGIN";
      case STATE_START_XMPP_LOGIN:  return "START_XMPP_LOGIN";
      default: return Task::GetStateName(state);
    }
  }

  int ProcessTokenLogin();
  int ProcessStartXmppLogin();
  void EnsureClosed();

  class Private;
  friend class Private;
  talk_base::scoped_ptr<Private> d_;

  bool delivering_signal_;
  bool valid_;
};

}

#endif  // TALK_XMPP_XMPPCLIENT_H_
