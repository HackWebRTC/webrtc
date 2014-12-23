/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_SESSION_SESSIONSENDTASK_H_
#define WEBRTC_LIBJINGLE_SESSION_SESSIONSENDTASK_H_

#include "webrtc/libjingle/session/sessionmanager.h"
#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/libjingle/xmpp/xmppclient.h"
#include "webrtc/libjingle/xmpp/xmppengine.h"
#include "webrtc/libjingle/xmpp/xmpptask.h"
#include "webrtc/base/common.h"

namespace cricket {

// The job of this task is to send an IQ stanza out (after stamping it with
// an ID attribute) and then wait for a response.  If not response happens
// within 5 seconds, it will signal failure on a SessionManager.  If an error
// happens it will also signal failure.  If, however, the send succeeds this
// task will quietly go away.

class SessionSendTask : public buzz::XmppTask {
 public:
  SessionSendTask(buzz::XmppTaskParentInterface* parent,
                  SessionManager* session_manager)
    : buzz::XmppTask(parent, buzz::XmppEngine::HL_SINGLE),
      session_manager_(session_manager) {
    set_timeout_seconds(15);
    session_manager_->SignalDestroyed.connect(
        this, &SessionSendTask::OnSessionManagerDestroyed);
  }

  virtual ~SessionSendTask() {
    SignalDone(this);
  }

  void Send(const buzz::XmlElement* stanza) {
    ASSERT(stanza_.get() == NULL);

    // This should be an IQ of type set, result, or error.  In the first case,
    // we supply an ID.  In the others, it should be present.
    ASSERT(stanza->Name() == buzz::QN_IQ);
    ASSERT(stanza->HasAttr(buzz::QN_TYPE));
    if (stanza->Attr(buzz::QN_TYPE) == "set") {
      ASSERT(!stanza->HasAttr(buzz::QN_ID));
    } else {
      ASSERT((stanza->Attr(buzz::QN_TYPE) == "result") ||
             (stanza->Attr(buzz::QN_TYPE) == "error"));
      ASSERT(stanza->HasAttr(buzz::QN_ID));
    }

    stanza_.reset(new buzz::XmlElement(*stanza));
    if (stanza_->HasAttr(buzz::QN_ID)) {
      set_task_id(stanza_->Attr(buzz::QN_ID));
    } else {
      stanza_->SetAttr(buzz::QN_ID, task_id());
    }
  }

  void OnSessionManagerDestroyed() {
    // If the session manager doesn't exist anymore, we should still try to
    // send the message, but avoid calling back into the SessionManager.
    session_manager_ = NULL;
  }

  sigslot::signal1<SessionSendTask *> SignalDone;

 protected:
  virtual int OnTimeout() {
    if (session_manager_ != NULL) {
      session_manager_->OnFailedSend(stanza_.get(), NULL);
    }

    return XmppTask::OnTimeout();
  }

  virtual int ProcessStart() {
    SendStanza(stanza_.get());
    if (stanza_->Attr(buzz::QN_TYPE) == buzz::STR_SET) {
      return STATE_RESPONSE;
    } else {
      return STATE_DONE;
    }
  }

  virtual int ProcessResponse() {
    const buzz::XmlElement* next = NextStanza();
    if (next == NULL)
      return STATE_BLOCKED;

    if (session_manager_ != NULL) {
      if (next->Attr(buzz::QN_TYPE) == buzz::STR_RESULT) {
        session_manager_->OnIncomingResponse(stanza_.get(), next);
      } else {
        session_manager_->OnFailedSend(stanza_.get(), next);
      }
    }

    return STATE_DONE;
  }

  virtual bool HandleStanza(const buzz::XmlElement *stanza) {
    if (!MatchResponseIq(stanza,
                         buzz::Jid(stanza_->Attr(buzz::QN_TO)), task_id()))
      return false;
    if (stanza->Attr(buzz::QN_TYPE) == buzz::STR_RESULT ||
        stanza->Attr(buzz::QN_TYPE) == buzz::STR_ERROR) {
      QueueStanza(stanza);
      return true;
    }
    return false;
  }

 private:
  SessionManager *session_manager_;
  rtc::scoped_ptr<buzz::XmlElement> stanza_;
};

}

#endif // WEBRTC_P2P_CLIENT_SESSIONSENDTASK_H_
