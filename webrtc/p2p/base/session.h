/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_SESSION_H_
#define WEBRTC_P2P_BASE_SESSION_H_

#include <list>
#include <map>
#include <string>
#include <vector>

#include "webrtc/base/refcount.h"
#include "webrtc/base/rtccertificate.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/socketaddress.h"
#include "webrtc/p2p/base/candidate.h"
#include "webrtc/p2p/base/port.h"
#include "webrtc/p2p/base/transport.h"

namespace cricket {

class BaseSession;
class P2PTransportChannel;
class Transport;
class TransportChannel;
class TransportChannelImpl;
class TransportController;

// Statistics for all the transports of this session.
typedef std::map<std::string, TransportStats> TransportStatsMap;
typedef std::map<std::string, std::string> ProxyTransportMap;

// TODO(pthatcher): Think of a better name for this.  We already have
// a TransportStats in transport.h.  Perhaps TransportsStats?
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
                    public rtc::MessageHandler {
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

  BaseSession(rtc::Thread* signaling_thread,
              rtc::Thread* worker_thread,
              PortAllocator* port_allocator,
              const std::string& sid,
              bool initiator);
  virtual ~BaseSession();

  // These are const to allow them to be called from const methods.
  rtc::Thread* signaling_thread() const { return signaling_thread_; }
  rtc::Thread* worker_thread() const { return worker_thread_; }
  PortAllocator* port_allocator() const { return port_allocator_; }

  // The ID of this session.
  const std::string& id() const { return sid_; }

  // Returns the application-level description given by our client.
  // If we are the recipient, this will be NULL until we send an accept.
  const SessionDescription* local_description() const;

  // Returns the application-level description given by the other client.
  // If we are the initiator, this will be NULL until we receive an accept.
  const SessionDescription* remote_description() const;

  SessionDescription* remote_description();

  // Takes ownership of SessionDescription*
  void set_local_description(const SessionDescription* sdesc);

  // Takes ownership of SessionDescription*
  void set_remote_description(SessionDescription* sdesc);

  void set_initiator(bool initiator);
  bool initiator() const { return initiator_; }

  const SessionDescription* initiator_description() const;

  // Returns the current state of the session.  See the enum above for details.
  // Each time the state changes, we will fire this signal.
  State state() const { return state_; }
  sigslot::signal2<BaseSession* , State> SignalState;

  // Returns the last error in the session.  See the enum above for details.
  // Each time the an error occurs, we will fire this signal.
  Error error() const { return error_; }
  const std::string& error_desc() const { return error_desc_; }
  sigslot::signal2<BaseSession* , Error> SignalError;

  // Updates the state, signaling if necessary.
  virtual void SetState(State state);

  // Updates the error state, signaling if necessary.
  // TODO(ronghuawu): remove the SetError method that doesn't take |error_desc|.
  virtual void SetError(Error error, const std::string& error_desc);

  void SetIceConnectionReceivingTimeout(int timeout_ms);

  // Start gathering candidates for any new transports, or transports doing an
  // ICE restart.
  void MaybeStartGathering();

 protected:
  bool PushdownTransportDescription(ContentSource source,
                                    ContentAction action,
                                    std::string* error_desc);

  // Handles messages posted to us.
  virtual void OnMessage(rtc::Message *pmsg);

  TransportController* transport_controller() {
    return transport_controller_.get();
  }

 protected:
  State state_;
  Error error_;
  std::string error_desc_;

 private:
  // Helper methods to push local and remote transport descriptions.
  bool PushdownLocalTransportDescription(
      const SessionDescription* sdesc, ContentAction action,
      std::string* error_desc);
  bool PushdownRemoteTransportDescription(
      const SessionDescription* sdesc, ContentAction action,
      std::string* error_desc);

  // Log session state.
  void LogState(State old_state, State new_state);

  // Returns true and the TransportInfo of the given |content_name|
  // from |description|. Returns false if it's not available.
  static bool GetTransportDescription(const SessionDescription* description,
                                      const std::string& content_name,
                                      TransportDescription* info);

  rtc::Thread* const signaling_thread_;
  rtc::Thread* const worker_thread_;
  PortAllocator* const port_allocator_;
  const std::string sid_;
  bool initiator_;
  rtc::scoped_ptr<TransportController> transport_controller_;
  rtc::scoped_ptr<const SessionDescription> local_description_;
  rtc::scoped_ptr<SessionDescription> remote_description_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_SESSION_H_
