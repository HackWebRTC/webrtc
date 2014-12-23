/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_SESSION_SESSIONMANAGER_H_
#define WEBRTC_LIBJINGLE_SESSION_SESSIONMANAGER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "webrtc/base/sigslot.h"
#include "webrtc/base/thread.h"
#include "webrtc/libjingle/session/parsing.h"
#include "webrtc/libjingle/session/sessionclient.h"
#include "webrtc/libjingle/session/sessionmessages.h"
#include "webrtc/libjingle/xmllite/xmlelement.h"
#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/p2p/base/portallocator.h"
#include "webrtc/p2p/base/session.h"
#include "webrtc/p2p/base/transportdescriptionfactory.h"

namespace buzz {
class QName;
class XmlElement;
}

namespace cricket {

class BaseSession;
class SessionClient;
class SessionManager;

// Used for errors that will send back a specific error message to the
// remote peer.  We add "type" to the errors because it's needed for
// SignalErrorMessage.
struct MessageError : ParseError {
  buzz::QName type;

  // if unset, assume type is a parse error
  MessageError() : ParseError(), type(buzz::QN_STANZA_BAD_REQUEST) {}

  void SetType(const buzz::QName type) {
    this->type = type;
  }
};

// Used for errors that may be returned by public session methods that
// can fail.
// TODO: Use this error in Session::Initiate and
// Session::Accept.
struct SessionError : WriteError {
};

// A specific Session created by the SessionManager, using XMPP for protocol.
class Session : public BaseSession {
 public:
  // Returns the manager that created and owns this session.
  SessionManager* session_manager() const { return session_manager_; }

  // Returns the client that is handling the application data of this session.
  SessionClient* client() const { return client_; }

    // Returns the JID of this client.
  const std::string& local_name() const { return local_name_; }

  // Returns the JID of the other peer in this session.
  const std::string& remote_name() const { return remote_name_; }

  // Set the JID of the other peer in this session.
  // Typically the remote_name_ is set when the session is initiated.
  // However, sometimes (e.g when a proxy is used) the peer name is
  // known after the BaseSession has been initiated and it must be updated
  // explicitly.
  void set_remote_name(const std::string& name) { remote_name_ = name; }

  // Set the JID of the initiator of this session. Allows for the overriding
  // of the initiator to be a third-party, eg. the MUC JID when creating p2p
  // sessions.
  void set_initiator_name(const std::string& name) { initiator_name_ = name; }

  // Indicates the JID of the entity who initiated this session.
  // In special cases, may be different than both local_name and remote_name.
  const std::string& initiator_name() const { return initiator_name_; }

  SignalingProtocol current_protocol() const { return current_protocol_; }

  void set_current_protocol(SignalingProtocol protocol) {
    current_protocol_ = protocol;
  }

  // Updates the error state, signaling if necessary.
  virtual void SetError(Error error, const std::string& error_desc);

  // When the session needs to send signaling messages, it beings by requesting
  // signaling.  The client should handle this by calling OnSignalingReady once
  // it is ready to send the messages.
  // (These are called only by SessionManager.)
  sigslot::signal1<Session*> SignalRequestSignaling;
  void OnSignalingReady() { BaseSession::OnSignalingReady(); }

  // Takes ownership of session description.
  // TODO: Add an error argument to pass back to the caller.
  bool Initiate(const std::string& to,
                const SessionDescription* sdesc);

  // When we receive an initiate, we create a session in the
  // RECEIVEDINITIATE state and respond by accepting or rejecting.
  // Takes ownership of session description.
  // TODO: Add an error argument to pass back to the caller.
  bool Accept(const SessionDescription* sdesc);
  bool Reject(const std::string& reason);
  bool Terminate() {
    return TerminateWithReason(STR_TERMINATE_SUCCESS);
  }
  bool TerminateWithReason(const std::string& reason);
  // Fired whenever we receive a terminate message along with a reason
  sigslot::signal2<Session*, const std::string&> SignalReceivedTerminateReason;

  // The two clients in the session may also send one another
  // arbitrary XML messages, which are called "info" messages. Sending
  // takes ownership of the given elements.  The signal does not; the
  // parent element will be deleted after the signal.
  bool SendInfoMessage(const XmlElements& elems,
                       const std::string& remote_name);
  bool SendDescriptionInfoMessage(const ContentInfos& contents);
  sigslot::signal2<Session*, const buzz::XmlElement*> SignalInfoMessage;

 private:
  // Creates or destroys a session.  (These are called only SessionManager.)
  Session(SessionManager *session_manager,
          const std::string& local_name, const std::string& initiator_name,
          const std::string& sid, const std::string& content_type,
          SessionClient* client);
  ~Session();
  // For each transport info, create a transport proxy.  Can fail for
  // incompatible transport types.
  bool CreateTransportProxies(const TransportInfos& tinfos,
                              SessionError* error);
  bool OnRemoteCandidates(const TransportInfos& tinfos,
                          ParseError* error);
  // Returns a TransportInfo without candidates for each content name.
  // Uses the transport_type_ of the session.
  TransportInfos GetEmptyTransportInfos(const ContentInfos& contents) const;

    // Maps passed to serialization functions.
  TransportParserMap GetTransportParsers();
  ContentParserMap GetContentParsers();
  CandidateTranslatorMap GetCandidateTranslators();

  virtual void OnTransportRequestSignaling(Transport* transport);
  virtual void OnTransportConnecting(Transport* transport);
  virtual void OnTransportWritable(Transport* transport);
  virtual void OnTransportProxyCandidatesReady(TransportProxy* proxy,
                                               const Candidates& candidates);
  virtual void OnMessage(rtc::Message *pmsg);

  // Send various kinds of session messages.
  bool SendInitiateMessage(const SessionDescription* sdesc,
                           SessionError* error);
  bool SendAcceptMessage(const SessionDescription* sdesc, SessionError* error);
  bool SendRejectMessage(const std::string& reason, SessionError* error);
  bool SendTerminateMessage(const std::string& reason, SessionError* error);
  bool SendTransportInfoMessage(const TransportInfo& tinfo,
                                SessionError* error);
  bool SendTransportInfoMessage(const TransportProxy* transproxy,
                                const Candidates& candidates,
                                SessionError* error);

  bool ResendAllTransportInfoMessages(SessionError* error);
  bool SendAllUnsentTransportInfoMessages(SessionError* error);

  // All versions of SendMessage send a message of the given type to
  // the other client.  Can pass either a set of elements or an
  // "action", which must have a WriteSessionAction method to go along
  // with it.  Sending with an action supports sending a "hybrid"
  // message.  Sending with elements must be sent as Jingle or Gingle.

  // When passing elems, must be either Jingle or Gingle protocol.
  // Takes ownership of action_elems.
  bool SendMessage(ActionType type, const XmlElements& action_elems,
                   SessionError* error);
  // Sends a messge, but overrides the remote name.
  bool SendMessage(ActionType type, const XmlElements& action_elems,
                   const std::string& remote_name,
                   SessionError* error);
  // When passing an action, may be Hybrid protocol.
  template <typename Action>
  bool SendMessage(ActionType type, const Action& action,
                   SessionError* error);

  // Helper methods to write the session message stanza.
  template <typename Action>
  bool WriteActionMessage(ActionType type, const Action& action,
                          buzz::XmlElement* stanza, WriteError* error);
  template <typename Action>
  bool WriteActionMessage(SignalingProtocol protocol,
                          ActionType type, const Action& action,
                          buzz::XmlElement* stanza, WriteError* error);

  // Sending messages in hybrid form requires being able to write them
  // on a per-protocol basis with a common method signature, which all
  // of these have.
  bool WriteSessionAction(SignalingProtocol protocol,
                          const SessionInitiate& init,
                          XmlElements* elems, WriteError* error);
  bool WriteSessionAction(SignalingProtocol protocol,
                          const TransportInfo& tinfo,
                          XmlElements* elems, WriteError* error);
  bool WriteSessionAction(SignalingProtocol protocol,
                          const SessionTerminate& term,
                          XmlElements* elems, WriteError* error);

  // Sends a message back to the other client indicating that we have received
  // and accepted their message.
  void SendAcknowledgementMessage(const buzz::XmlElement* stanza);

  // Once signaling is ready, the session will use this signal to request the
  // sending of each message.  When messages are received by the other client,
  // they should be handed to OnIncomingMessage.
  // (These are called only by SessionManager.)
  sigslot::signal2<Session* , const buzz::XmlElement*> SignalOutgoingMessage;
  void OnIncomingMessage(const SessionMessage& msg);

  void OnIncomingResponse(const buzz::XmlElement* orig_stanza,
                          const buzz::XmlElement* response_stanza,
                          const SessionMessage& msg);
  void OnInitiateAcked();
  void OnFailedSend(const buzz::XmlElement* orig_stanza,
                    const buzz::XmlElement* error_stanza);

  // Invoked when an error is found in an incoming message.  This is translated
  // into the appropriate XMPP response by SessionManager.
  sigslot::signal6<BaseSession*,
                   const buzz::XmlElement*,
                   const buzz::QName&,
                   const std::string&,
                   const std::string&,
                   const buzz::XmlElement*> SignalErrorMessage;

  // Handlers for the various types of messages.  These functions may take
  // pointers to the whole stanza or to just the session element.
  bool OnInitiateMessage(const SessionMessage& msg, MessageError* error);
  bool OnAcceptMessage(const SessionMessage& msg, MessageError* error);
  bool OnRejectMessage(const SessionMessage& msg, MessageError* error);
  bool OnInfoMessage(const SessionMessage& msg);
  bool OnTerminateMessage(const SessionMessage& msg, MessageError* error);
  bool OnTransportInfoMessage(const SessionMessage& msg, MessageError* error);
  bool OnTransportAcceptMessage(const SessionMessage& msg, MessageError* error);
  bool OnDescriptionInfoMessage(const SessionMessage& msg, MessageError* error);
  bool OnRedirectError(const SessionRedirect& redirect, SessionError* error);

  // Verifies that we are in the appropriate state to receive this message.
  bool CheckState(State state, MessageError* error);

  SessionManager* session_manager_;
  bool initiate_acked_;
  std::string local_name_;
  std::string initiator_name_;
  std::string remote_name_;
  SessionClient* client_;
  TransportParser* transport_parser_;
  // Keeps track of what protocol we are speaking.
  SignalingProtocol current_protocol_;

  friend class SessionManager;  // For access to constructor, destructor,
                                // and signaling related methods.
};

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

#endif  // WEBRTC_LIBJINGLE_SESSION_SESSIONMANAGER_H_
