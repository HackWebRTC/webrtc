/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_SESSIONMANAGER_H_
#define WEBRTC_P2P_BASE_SESSIONMANAGER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "webrtc/p2p/base/portallocator.h"
#include "webrtc/p2p/base/transportdescriptionfactory.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/thread.h"

namespace buzz {
class QName;
class XmlElement;
}

namespace cricket {

class Session;
class BaseSession;
class SessionClient;

// SessionManager manages session instances.
class SessionManager : public sigslot::has_slots<> {
 public:
  SessionManager(PortAllocator *allocator,
                 rtc::Thread *worker_thread = NULL);
  virtual ~SessionManager();

  PortAllocator *port_allocator() const { return allocator_; }
  rtc::Thread *worker_thread() const { return worker_thread_; }
  rtc::Thread *signaling_thread() const { return signaling_thread_; }

  int session_timeout() const { return timeout_; }
  void set_session_timeout(int timeout) { timeout_ = timeout; }

  // Set what transport protocol we want to default to.
  void set_transport_protocol(TransportProtocol proto) {
     transport_desc_factory_.set_protocol(proto);
  }

  // Control use of DTLS. An identity must be supplied if DTLS is enabled.
  void set_secure(SecurePolicy policy) {
    transport_desc_factory_.set_secure(policy);
  }
  void set_identity(rtc::SSLIdentity* identity) {
    transport_desc_factory_.set_identity(identity);
  }
  const TransportDescriptionFactory* transport_desc_factory() const {
    return &transport_desc_factory_;
  }

  // Registers support for the given client.  If we receive an initiate
  // describing a session of the given type, we will automatically create a
  // Session object and notify this client.  The client may then accept or
  // reject the session.
  void AddClient(const std::string& content_type, SessionClient* client);
  void RemoveClient(const std::string& content_type);
  SessionClient* GetClient(const std::string& content_type);

  // Creates a new session.  The given name is the JID of the client on whose
  // behalf we initiate the session.
  Session *CreateSession(const std::string& local_name,
                         const std::string& content_type);

  Session *CreateSession(const std::string& id,
                         const std::string& local_name,
                         const std::string& content_type);

  // Destroys the given session.
  void DestroySession(Session *session);

  // Returns the session with the given ID or NULL if none exists.
  Session *GetSession(const std::string& sid);

  // Terminates all of the sessions created by this manager.
  void TerminateAll();

  // These are signaled whenever the set of existing sessions changes.
  sigslot::signal2<Session *, bool> SignalSessionCreate;
  sigslot::signal1<Session *> SignalSessionDestroy;

  // Determines whether the given stanza is intended for some session.
  bool IsSessionMessage(const buzz::XmlElement* stanza);

  // Given a sid, initiator, and remote_name, this finds the matching Session
  Session* FindSession(const std::string& sid,
                       const std::string& remote_name);

  // Called when we receive a stanza for which IsSessionMessage is true.
  void OnIncomingMessage(const buzz::XmlElement* stanza);

  // Called when we get a response to a message that we sent.
  void OnIncomingResponse(const buzz::XmlElement* orig_stanza,
                          const buzz::XmlElement* response_stanza);

  // Called if an attempted to send times out or an error is returned.  In the
  // timeout case error_stanza will be NULL
  void OnFailedSend(const buzz::XmlElement* orig_stanza,
                    const buzz::XmlElement* error_stanza);

  // Signalled each time a session generates a signaling message to send.
  // Also signalled on errors, but with a NULL session.
  sigslot::signal2<SessionManager*,
                   const buzz::XmlElement*> SignalOutgoingMessage;

  // Signaled before sessions try to send certain signaling messages.  The
  // client should call OnSignalingReady once it is safe to send them.  These
  // steps are taken so that we don't send signaling messages trying to
  // re-establish the connectivity of a session when the client cannot send
  // the messages (and would probably just drop them on the floor).
  //
  // Note: you can connect this directly to OnSignalingReady(), if a signalling
  // check is not supported.
  sigslot::signal0<> SignalRequestSignaling;
  void OnSignalingReady();

  // Signaled when this SessionManager is deleted.
  sigslot::signal0<> SignalDestroyed;

 private:
  typedef std::map<std::string, Session*> SessionMap;
  typedef std::map<std::string, SessionClient*> ClientMap;

  // Helper function for CreateSession.  This is also invoked when we receive
  // a message attempting to initiate a session with this client.
  Session *CreateSession(const std::string& local_name,
                         const std::string& initiator,
                         const std::string& sid,
                         const std::string& content_type,
                         bool received_initiate);

  // Attempts to find a registered session type whose description appears as
  // a child of the session element.  Such a child should be present indicating
  // the application they hope to initiate.
  std::string FindClient(const buzz::XmlElement* session);

  // Sends a message back to the other client indicating that we found an error
  // in the stanza they sent.  name identifies the error, type is one of the
  // standard XMPP types (cancel, continue, modify, auth, wait), and text is a
  // description for debugging purposes.
  void SendErrorMessage(const buzz::XmlElement* stanza,
                        const buzz::QName& name,
                        const std::string& type,
                        const std::string& text,
                        const buzz::XmlElement* extra_info);

  // Creates and returns an error message from the given components.  The
  // caller is responsible for deleting this.
  buzz::XmlElement* CreateErrorMessage(
      const buzz::XmlElement* stanza,
      const buzz::QName& name,
      const std::string& type,
      const std::string& text,
      const buzz::XmlElement* extra_info);

  // Called each time a session requests signaling.
  void OnRequestSignaling(Session* session);

  // Called each time a session has an outgoing message.
  void OnOutgoingMessage(Session* session, const buzz::XmlElement* stanza);

  // Called each time a session has an error to send.
  void OnErrorMessage(BaseSession* session,
                      const buzz::XmlElement* stanza,
                      const buzz::QName& name,
                      const std::string& type,
                      const std::string& text,
                      const buzz::XmlElement* extra_info);

  PortAllocator *allocator_;
  rtc::Thread *signaling_thread_;
  rtc::Thread *worker_thread_;
  int timeout_;
  TransportDescriptionFactory transport_desc_factory_;
  SessionMap session_map_;
  ClientMap client_map_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_SESSIONMANAGER_H_
