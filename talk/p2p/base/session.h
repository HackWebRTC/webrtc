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

#ifndef TALK_P2P_BASE_SESSION_H_
#define TALK_P2P_BASE_SESSION_H_

#include <list>
#include <map>
#include <string>
#include <vector>

#include "talk/base/refcount.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/scoped_ref_ptr.h"
#include "talk/base/socketaddress.h"
#include "talk/p2p/base/parsing.h"
#include "talk/p2p/base/port.h"
#include "talk/p2p/base/sessionclient.h"
#include "talk/p2p/base/sessionmanager.h"
#include "talk/p2p/base/sessionmessages.h"
#include "talk/p2p/base/transport.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/constants.h"

namespace cricket {

class BaseSession;
class P2PTransportChannel;
class Transport;
class TransportChannel;
class TransportChannelProxy;
class TransportChannelImpl;

typedef talk_base::RefCountedObject<talk_base::scoped_ptr<Transport> >
TransportWrapper;

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

// Bundles a Transport and ChannelMap together. ChannelMap is used to
// create transport channels before receiving or sending a session
// initiate, and for speculatively connecting channels.  Previously, a
// session had one ChannelMap and transport.  Now, with multiple
// transports per session, we need multiple ChannelMaps as well.

typedef std::map<int, TransportChannelProxy*> ChannelMap;

class TransportProxy : public sigslot::has_slots<>,
                       public CandidateTranslator {
 public:
  TransportProxy(
      const std::string& sid,
      const std::string& content_name,
      TransportWrapper* transport)
      : sid_(sid),
        content_name_(content_name),
        transport_(transport),
        connecting_(false),
        negotiated_(false),
        sent_candidates_(false),
        candidates_allocated_(false) {
    transport_->get()->SignalCandidatesReady.connect(
        this, &TransportProxy::OnTransportCandidatesReady);
  }
  ~TransportProxy();

  std::string content_name() const { return content_name_; }
  // TODO(juberti): It's not good form to expose the object you're wrapping,
  // since callers can mutate it. Can we make this return a const Transport*?
  Transport* impl() const { return transport_->get(); }

  std::string type() const;
  bool negotiated() const { return negotiated_; }
  const Candidates& sent_candidates() const { return sent_candidates_; }
  const Candidates& unsent_candidates() const { return unsent_candidates_; }
  bool candidates_allocated() const { return candidates_allocated_; }
  void set_candidates_allocated(bool allocated) {
    candidates_allocated_ = allocated;
  }

  TransportChannel* GetChannel(int component);
  TransportChannel* CreateChannel(const std::string& channel_name,
                                  int component);
  bool HasChannel(int component);
  void DestroyChannel(int component);

  void AddSentCandidates(const Candidates& candidates);
  void AddUnsentCandidates(const Candidates& candidates);
  void ClearSentCandidates() { sent_candidates_.clear(); }
  void ClearUnsentCandidates() { unsent_candidates_.clear(); }

  // Start the connection process for any channels, creating impls if needed.
  void ConnectChannels();
  // Hook up impls to the proxy channels. Doesn't change connect state.
  void CompleteNegotiation();

  // Mux this proxy onto the specified proxy's transport.
  bool SetupMux(TransportProxy* proxy);

  // Simple functions that thunk down to the same functions on Transport.
  void SetRole(TransportRole role);
  bool SetLocalTransportDescription(const TransportDescription& description,
                                    ContentAction action);
  bool SetRemoteTransportDescription(const TransportDescription& description,
                                     ContentAction action);
  void OnSignalingReady();
  bool OnRemoteCandidates(const Candidates& candidates, std::string* error);

  // CandidateTranslator methods.
  virtual bool GetChannelNameFromComponent(
      int component, std::string* channel_name) const;
  virtual bool GetComponentFromChannelName(
      const std::string& channel_name, int* component) const;

  // Called when a transport signals that it has new candidates.
  void OnTransportCandidatesReady(cricket::Transport* transport,
                                  const Candidates& candidates) {
    SignalCandidatesReady(this, candidates);
  }

  // Handles sending of ready candidates and receiving of remote candidates.
  sigslot::signal2<TransportProxy*,
                         const std::vector<Candidate>&> SignalCandidatesReady;

 private:
  TransportChannelProxy* GetChannelProxy(int component) const;
  TransportChannelProxy* GetChannelProxyByName(const std::string& name) const;
  void ReplaceChannelProxyImpl(TransportChannelProxy* channel_proxy,
                               size_t index);
  TransportChannelImpl* GetOrCreateChannelProxyImpl(int component);
  void SetChannelProxyImpl(int component,
                           TransportChannelProxy* proxy);

  std::string sid_;
  std::string content_name_;
  talk_base::scoped_refptr<TransportWrapper> transport_;
  bool connecting_;
  bool negotiated_;
  ChannelMap channels_;
  Candidates sent_candidates_;
  Candidates unsent_candidates_;
  bool candidates_allocated_;
};

typedef std::map<std::string, TransportProxy*> TransportMap;

// Statistics for all the transports of this session.
typedef std::map<std::string, TransportStats> TransportStatsMap;
typedef std::map<std::string, std::string> ProxyTransportMap;

struct SessionStats {
  ProxyTransportMap proxy_to_transport;
  TransportStatsMap transport_stats;
};

// A BaseSession manages general session state. This includes negotiation
// of both the application-level and network-level protocols:  the former
// defines what will be sent and the latter defines how it will be sent.  Each
// network-level protocol is represented by a Transport object.  Each Transport
// participates in the network-level negotiation.  The individual streams of
// packets are represented by TransportChannels.  The application-level protocol
// is represented by SessionDecription objects.
class BaseSession : public sigslot::has_slots<>,
                    public talk_base::MessageHandler {
 public:
  enum {
    MSG_TIMEOUT = 0,
    MSG_ERROR,
    MSG_STATE,
  };

  enum State {
    STATE_INIT = 0,
    STATE_SENTINITIATE,       // sent initiate, waiting for Accept or Reject
    STATE_RECEIVEDINITIATE,   // received an initiate. Call Accept or Reject
    STATE_SENTPRACCEPT,       // sent provisional Accept
    STATE_SENTACCEPT,         // sent accept. begin connecting transport
    STATE_RECEIVEDPRACCEPT,   // received provisional Accept, waiting for Accept
    STATE_RECEIVEDACCEPT,     // received accept. begin connecting transport
    STATE_SENTMODIFY,         // sent modify, waiting for Accept or Reject
    STATE_RECEIVEDMODIFY,     // received modify, call Accept or Reject
    STATE_SENTREJECT,         // sent reject after receiving initiate
    STATE_RECEIVEDREJECT,     // received reject after sending initiate
    STATE_SENTREDIRECT,       // sent direct after receiving initiate
    STATE_SENTTERMINATE,      // sent terminate (any time / either side)
    STATE_RECEIVEDTERMINATE,  // received terminate (any time / either side)
    STATE_INPROGRESS,         // session accepted and in progress
    STATE_DEINIT,             // session is being destroyed
  };

  enum Error {
    ERROR_NONE = 0,       // no error
    ERROR_TIME = 1,       // no response to signaling
    ERROR_RESPONSE = 2,   // error during signaling
    ERROR_NETWORK = 3,    // network error, could not allocate network resources
    ERROR_CONTENT = 4,    // channel errors in SetLocalContent/SetRemoteContent
    ERROR_TRANSPORT = 5,  // transport error of some kind
  };

  // Convert State to a readable string.
  static std::string StateToString(State state);

  BaseSession(talk_base::Thread* signaling_thread,
              talk_base::Thread* worker_thread,
              PortAllocator* port_allocator,
              const std::string& sid,
              const std::string& content_type,
              bool initiator);
  virtual ~BaseSession();

  talk_base::Thread* signaling_thread() { return signaling_thread_; }
  talk_base::Thread* worker_thread() { return worker_thread_; }
  PortAllocator* port_allocator() { return port_allocator_; }

  // The ID of this session.
  const std::string& id() const { return sid_; }

  // TODO(juberti): This data is largely redundant, as it can now be obtained
  // from local/remote_description(). Remove these functions and members.
  // Returns the XML namespace identifying the type of this session.
  const std::string& content_type() const { return content_type_; }
  // Returns the XML namespace identifying the transport used for this session.
  const std::string& transport_type() const { return transport_type_; }

  // Indicates whether we initiated this session.
  bool initiator() const { return initiator_; }

  // Returns the application-level description given by our client.
  // If we are the recipient, this will be NULL until we send an accept.
  const SessionDescription* local_description() const {
    return local_description_;
  }
  // Returns the application-level description given by the other client.
  // If we are the initiator, this will be NULL until we receive an accept.
  const SessionDescription* remote_description() const {
    return remote_description_;
  }
  SessionDescription* remote_description() {
    return remote_description_;
  }

  // Takes ownership of SessionDescription*
  bool set_local_description(const SessionDescription* sdesc) {
    if (sdesc != local_description_) {
      delete local_description_;
      local_description_ = sdesc;
    }
    return true;
  }

  // Takes ownership of SessionDescription*
  bool set_remote_description(SessionDescription* sdesc) {
    if (sdesc != remote_description_) {
      delete remote_description_;
      remote_description_ = sdesc;
    }
    return true;
  }

  const SessionDescription* initiator_description() const {
    if (initiator_) {
      return local_description_;
    } else {
      return remote_description_;
    }
  }

  // Returns the current state of the session.  See the enum above for details.
  // Each time the state changes, we will fire this signal.
  State state() const { return state_; }
  sigslot::signal2<BaseSession* , State> SignalState;

  // Returns the last error in the session.  See the enum above for details.
  // Each time the an error occurs, we will fire this signal.
  Error error() const { return error_; }
  sigslot::signal2<BaseSession* , Error> SignalError;

  // Updates the state, signaling if necessary.
  virtual void SetState(State state);

  // Updates the error state, signaling if necessary.
  virtual void SetError(Error error);

  // Fired when the remote description is updated, with the updated
  // contents.
  sigslot::signal2<BaseSession* , const ContentInfos&>
      SignalRemoteDescriptionUpdate;

  // Fired when SetState is called (regardless if there's a state change), which
  // indicates the session description might have be updated.
  sigslot::signal2<BaseSession*, ContentAction> SignalNewLocalDescription;

  // Fired when SetState is called (regardless if there's a state change), which
  // indicates the session description might have be updated.
  sigslot::signal2<BaseSession*, ContentAction> SignalNewRemoteDescription;

  // Returns the transport that has been negotiated or NULL if
  // negotiation is still in progress.
  Transport* GetTransport(const std::string& content_name);

  // Creates a new channel with the given names.  This method may be called
  // immediately after creating the session.  However, the actual
  // implementation may not be fixed until transport negotiation completes.
  // This will usually be called from the worker thread, but that
  // shouldn't be an issue since the main thread will be blocked in
  // Send when doing so.
  virtual TransportChannel* CreateChannel(const std::string& content_name,
                                          const std::string& channel_name,
                                          int component);

  // Returns the channel with the given names.
  virtual TransportChannel* GetChannel(const std::string& content_name,
                                       int component);

  // Destroys the channel with the given names.
  // This will usually be called from the worker thread, but that
  // shouldn't be an issue since the main thread will be blocked in
  // Send when doing so.
  virtual void DestroyChannel(const std::string& content_name,
                              int component);

  // Returns stats for all channels of all transports.
  // This avoids exposing the internal structures used to track them.
  virtual bool GetStats(SessionStats* stats);

 protected:
  bool PushdownTransportDescription(ContentSource source,
                                    ContentAction action);
  void set_initiator(bool initiator) { initiator_ = initiator; }

  talk_base::SSLIdentity* identity() { return identity_; }
  // Specifies the identity to use in this session.
  void set_identity(talk_base::SSLIdentity* identity) { identity_ = identity; }

  const TransportMap& transport_proxies() const { return transports_; }
  // Get a TransportProxy by content_name or transport. NULL if not found.
  TransportProxy* GetTransportProxy(const std::string& content_name);
  TransportProxy* GetTransportProxy(const Transport* transport);
  TransportProxy* GetFirstTransportProxy();
  void DestroyTransportProxy(const std::string& content_name);
  // TransportProxy is owned by session.  Return proxy just for convenience.
  TransportProxy* GetOrCreateTransportProxy(const std::string& content_name);
  // Creates the actual transport object. Overridable for testing.
  virtual Transport* CreateTransport(const std::string& content_name);

  void OnSignalingReady();
  void SpeculativelyConnectAllTransportChannels();
  // Helper method to provide remote candidates to the transport.
  bool OnRemoteCandidates(const std::string& content_name,
                          const Candidates& candidates,
                          std::string* error);

  // This method will mux transport channels by content_name.
  // First content is used for muxing.
  bool MaybeEnableMuxingSupport();

  // Called when a transport requests signaling.
  virtual void OnTransportRequestSignaling(Transport* transport) {
  }

  // Called when the first channel of a transport begins connecting.  We use
  // this to start a timer, to make sure that the connection completes in a
  // reasonable amount of time.
  virtual void OnTransportConnecting(Transport* transport) {
  }

  // Called when a transport changes its writable state.  We track this to make
  // sure that the transport becomes writable within a reasonable amount of
  // time.  If this does not occur, we signal an error.
  virtual void OnTransportWritable(Transport* transport) {
  }
  virtual void OnTransportReadable(Transport* transport) {
  }

  // Called when a transport signals that it has new candidates.
  virtual void OnTransportProxyCandidatesReady(TransportProxy* proxy,
                                               const Candidates& candidates) {
  }

  // Called when a transport signals that it found an error in an incoming
  // message.
  virtual void OnTransportSendError(Transport* transport,
                                    const buzz::XmlElement* stanza,
                                    const buzz::QName& name,
                                    const std::string& type,
                                    const std::string& text,
                                    const buzz::XmlElement* extra_info) {
  }

  virtual void OnTransportRouteChange(
      Transport* transport,
      int component,
      const cricket::Candidate& remote_candidate) {
  }

  virtual void OnTransportCandidatesAllocationDone(Transport* transport);

  // Called when all transport channels allocated required candidates.
  // This method should be used as an indication of candidates gathering process
  // is completed and application can now send local candidates list to remote.
  virtual void OnCandidatesAllocationDone() {
  }

  // Handles the ice role change callback from Transport. This must be
  // propagated to all the transports.
  virtual void OnRoleConflict();

  // Handles messages posted to us.
  virtual void OnMessage(talk_base::Message *pmsg);

 protected:
  State state_;
  Error error_;

 private:
  // Helper methods to push local and remote transport descriptions.
  bool PushdownLocalTransportDescription(
      const SessionDescription* sdesc, ContentAction action);
  bool PushdownRemoteTransportDescription(
      const SessionDescription* sdesc, ContentAction action);

  bool IsCandidateAllocationDone() const;
  void MaybeCandidateAllocationDone();

  // This method will delete the Transport and TransportChannelImpls and
  // replace those with the selected Transport objects. Selection is done
  // based on the content_name and in this case first MediaContent information
  // is used for mux.
  bool SetSelectedProxy(const std::string& content_name,
                        const ContentGroup* muxed_group);
  // Log session state.
  void LogState(State old_state, State new_state);

  // Returns true and the TransportInfo of the given |content_name|
  // from |description|. Returns false if it's not available.
  bool GetTransportDescription(const SessionDescription* description,
                               const std::string& content_name,
                               TransportDescription* info);

  // Fires the new description signal according to the current state.
  void SignalNewDescription();

  // Gets the ContentAction and ContentSource according to the session state.
  bool GetContentAction(ContentAction* action, ContentSource* source);

  talk_base::Thread* signaling_thread_;
  talk_base::Thread* worker_thread_;
  PortAllocator* port_allocator_;
  std::string sid_;
  std::string content_type_;
  std::string transport_type_;
  bool initiator_;
  talk_base::SSLIdentity* identity_;
  const SessionDescription* local_description_;
  SessionDescription* remote_description_;
  uint64 ice_tiebreaker_;
  // This flag will be set to true after the first role switch. This flag
  // will enable us to stop any role switch during the call.
  bool role_switch_;
  TransportMap transports_;
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
  virtual void SetError(Error error);

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
  bool SendInfoMessage(const XmlElements& elems);
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
  virtual void OnTransportSendError(Transport* transport,
                                    const buzz::XmlElement* stanza,
                                    const buzz::QName& name,
                                    const std::string& type,
                                    const std::string& text,
                                    const buzz::XmlElement* extra_info);
  virtual void OnMessage(talk_base::Message *pmsg);

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

  // Both versions of SendMessage send a message of the given type to
  // the other client.  Can pass either a set of elements or an
  // "action", which must have a WriteSessionAction method to go along
  // with it.  Sending with an action supports sending a "hybrid"
  // message.  Sending with elements must be sent as Jingle or Gingle.

  // When passing elems, must be either Jingle or Gingle protocol.
  // Takes ownership of action_elems.
  bool SendMessage(ActionType type, const XmlElements& action_elems,
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

}  // namespace cricket

#endif  // TALK_P2P_BASE_SESSION_H_
