/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_SESSION_SESSIONMANAGERTASK_H_
#define WEBRTC_LIBJINGLE_SESSION_SESSIONMANAGERTASK_H_

#include "webrtc/libjingle/session/sessionmanager.h"
#include "webrtc/libjingle/session/sessionsendtask.h"
#include "webrtc/libjingle/xmpp/xmppengine.h"
#include "webrtc/libjingle/xmpp/xmpptask.h"

namespace cricket {

// This class handles sending and receiving XMPP messages on behalf of the
// SessionManager.  The sending part is handed over to SessionSendTask.

class SessionManagerTask : public buzz::XmppTask {
 public:
  SessionManagerTask(buzz::XmppTaskParentInterface* parent,
                     SessionManager* session_manager)
      : buzz::XmppTask(parent, buzz::XmppEngine::HL_SINGLE),
        session_manager_(session_manager) {
  }

  ~SessionManagerTask() {
  }

  // Turns on simple support for sending messages, using SessionSendTask.
  void EnableOutgoingMessages() {
    session_manager_->SignalOutgoingMessage.connect(
        this, &SessionManagerTask::OnOutgoingMessage);
    session_manager_->SignalRequestSignaling.connect(
        session_manager_, &SessionManager::OnSignalingReady);
  }

  virtual int ProcessStart() {
    const buzz::XmlElement *stanza = NextStanza();
    if (stanza == NULL)
      return STATE_BLOCKED;
    session_manager_->OnIncomingMessage(stanza);
    return STATE_START;
  }

 protected:
  virtual bool HandleStanza(const buzz::XmlElement *stanza) {
    if (!session_manager_->IsSessionMessage(stanza))
      return false;
    // Responses are handled by the SessionSendTask that sent the request.
    //if (stanza->Attr(buzz::QN_TYPE) != buzz::STR_SET)
    //  return false;
    QueueStanza(stanza);
    return true;
  }

 private:
  void OnOutgoingMessage(SessionManager* manager,
                         const buzz::XmlElement* stanza) {
    cricket::SessionSendTask* sender =
        new cricket::SessionSendTask(parent_, session_manager_);
    sender->Send(stanza);
    sender->Start();
  }

  SessionManager* session_manager_;
};

}  // namespace cricket

#endif // WEBRTC_P2P_CLIENT_SESSIONMANAGERTASK_H_
