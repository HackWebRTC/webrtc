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

// A Transport manages a set of named channels of the same type.
//
// Subclasses choose the appropriate class to instantiate for each channel;
// however, this base class keeps track of the channels by name, watches their
// state changes (in order to update the manager's state), and forwards
// requests to begin connecting or to reset to each of the channels.
//
// On Threading:  Transport performs work on both the signaling and worker
// threads.  For subclasses, the rule is that all signaling related calls will
// be made on the signaling thread and all channel related calls (including
// signaling for a channel) will be made on the worker thread.  When
// information needs to be sent between the two threads, this class should do
// the work (e.g., OnRemoteCandidate).
//
// Note: Subclasses must call DestroyChannels() in their own constructors.
// It is not possible to do so here because the subclass constructor will
// already have run.

#ifndef TALK_P2P_BASE_TRANSPORT_H_
#define TALK_P2P_BASE_TRANSPORT_H_

#include <string>
#include <map>
#include <vector>
#include "talk/base/criticalsection.h"
#include "talk/base/messagequeue.h"
#include "talk/base/sigslot.h"
#include "talk/p2p/base/candidate.h"
#include "talk/p2p/base/constants.h"
#include "talk/p2p/base/sessiondescription.h"
#include "talk/p2p/base/transportinfo.h"

namespace talk_base {
class Thread;
}

namespace buzz {
class QName;
class XmlElement;
}

namespace cricket {

struct ParseError;
struct WriteError;
class CandidateTranslator;
class PortAllocator;
class SessionManager;
class Session;
class TransportChannel;
class TransportChannelImpl;

typedef std::vector<buzz::XmlElement*> XmlElements;
typedef std::vector<Candidate> Candidates;

// Used to parse and serialize (write) transport candidates.  For
// convenience of old code, Transports will implement TransportParser.
// Parse/Write seems better than Serialize/Deserialize or
// Create/Translate.
class TransportParser {
 public:
  // The incoming Translator value may be null, in which case
  // ParseCandidates should return false if there are candidates to
  // parse (indicating a failure to parse).  If the Translator is null
  // and there are no candidates to parse, then return true,
  // indicating a successful parse of 0 candidates.

  // Parse or write a transport description, including ICE credentials and
  // any DTLS fingerprint. Since only Jingle has transport descriptions, these
  // functions are only used when serializing to Jingle.
  virtual bool ParseTransportDescription(const buzz::XmlElement* elem,
                                         const CandidateTranslator* translator,
                                         TransportDescription* tdesc,
                                         ParseError* error) = 0;
  virtual bool WriteTransportDescription(const TransportDescription& tdesc,
                                         const CandidateTranslator* translator,
                                         buzz::XmlElement** tdesc_elem,
                                         WriteError* error) = 0;


  // Parse a single candidate. This must be used when parsing Gingle
  // candidates, since there is no enclosing transport description.
  virtual bool ParseGingleCandidate(const buzz::XmlElement* elem,
                                    const CandidateTranslator* translator,
                                    Candidate* candidates,
                                    ParseError* error) = 0;
  virtual bool WriteGingleCandidate(const Candidate& candidate,
                                    const CandidateTranslator* translator,
                                    buzz::XmlElement** candidate_elem,
                                    WriteError* error) = 0;

  // Helper function to parse an element describing an address.  This
  // retrieves the IP and port from the given element and verifies
  // that they look like plausible values.
  bool ParseAddress(const buzz::XmlElement* elem,
                    const buzz::QName& address_name,
                    const buzz::QName& port_name,
                    talk_base::SocketAddress* address,
                    ParseError* error);

  virtual ~TransportParser() {}
};

// Whether our side of the call is driving the negotiation, or the other side.
enum TransportRole {
  ROLE_CONTROLLING = 0,
  ROLE_CONTROLLED,
  ROLE_UNKNOWN
};

// For "writable" and "readable", we need to differentiate between
// none, all, and some.
enum TransportState {
  TRANSPORT_STATE_NONE = 0,
  TRANSPORT_STATE_SOME,
  TRANSPORT_STATE_ALL
};

// Stats that we can return about the connections for a transport channel.
// TODO(hta): Rename to ConnectionStats
struct ConnectionInfo {
  bool best_connection;        // Is this the best connection we have?
  bool writable;               // Has this connection received a STUN response?
  bool readable;               // Has this connection received a STUN request?
  bool timeout;                // Has this connection timed out?
  bool new_connection;         // Is this a newly created connection?
  size_t rtt;                  // The STUN RTT for this connection.
  size_t sent_total_bytes;     // Total bytes sent on this connection.
  size_t sent_bytes_second;    // Bps over the last measurement interval.
  size_t recv_total_bytes;     // Total bytes received on this connection.
  size_t recv_bytes_second;    // Bps over the last measurement interval.
  Candidate local_candidate;   // The local candidate for this connection.
  Candidate remote_candidate;  // The remote candidate for this connection.
  void* key;                   // A static value that identifies this conn.
};

// Information about all the connections of a channel.
typedef std::vector<ConnectionInfo> ConnectionInfos;

// Information about a specific channel
struct TransportChannelStats {
  int component;
  ConnectionInfos connection_infos;
};

// Information about all the channels of a transport.
// TODO(hta): Consider if a simple vector is as good as a map.
typedef std::vector<TransportChannelStats> TransportChannelStatsList;

// Information about the stats of a transport.
struct TransportStats {
  std::string content_name;
  TransportChannelStatsList channel_stats;
};

class Transport : public talk_base::MessageHandler,
                  public sigslot::has_slots<> {
 public:
  Transport(talk_base::Thread* signaling_thread,
            talk_base::Thread* worker_thread,
            const std::string& content_name,
            const std::string& type,
            PortAllocator* allocator);
  virtual ~Transport();

  // Returns the signaling thread. The app talks to Transport on this thread.
  talk_base::Thread* signaling_thread() { return signaling_thread_; }
  // Returns the worker thread. The actual networking is done on this thread.
  talk_base::Thread* worker_thread() { return worker_thread_; }

  // Returns the content_name of this transport.
  const std::string& content_name() const { return content_name_; }
  // Returns the type of this transport.
  const std::string& type() const { return type_; }

  // Returns the port allocator object for this transport.
  PortAllocator* port_allocator() { return allocator_; }

  // Returns the readable and states of this manager.  These bits are the ORs
  // of the corresponding bits on the managed channels.  Each time one of these
  // states changes, a signal is raised.
  // TODO: Replace uses of readable() and writable() with
  // any_channels_readable() and any_channels_writable().
  bool readable() const { return any_channels_readable(); }
  bool writable() const { return any_channels_writable(); }
  bool was_writable() const { return was_writable_; }
  bool any_channels_readable() const {
    return (readable_ == TRANSPORT_STATE_SOME ||
            readable_ == TRANSPORT_STATE_ALL);
  }
  bool any_channels_writable() const {
    return (writable_ == TRANSPORT_STATE_SOME ||
            writable_ == TRANSPORT_STATE_ALL);
  }
  bool all_channels_readable() const {
    return (readable_ == TRANSPORT_STATE_ALL);
  }
  bool all_channels_writable() const {
    return (writable_ == TRANSPORT_STATE_ALL);
  }
  sigslot::signal1<Transport*> SignalReadableState;
  sigslot::signal1<Transport*> SignalWritableState;

  // Returns whether the client has requested the channels to connect.
  bool connect_requested() const { return connect_requested_; }

  void SetRole(TransportRole role);
  TransportRole role() const { return role_; }

  void SetTiebreaker(uint64 tiebreaker) { tiebreaker_ = tiebreaker; }
  uint64 tiebreaker() { return tiebreaker_; }

  TransportProtocol protocol() const { return protocol_; }

  // Create, destroy, and lookup the channels of this type by their components.
  TransportChannelImpl* CreateChannel(int component);
  // Note: GetChannel may lead to race conditions, since the mutex is not held
  // after the pointer is returned.
  TransportChannelImpl* GetChannel(int component);
  // Note: HasChannel does not lead to race conditions, unlike GetChannel.
  bool HasChannel(int component) {
    return (NULL != GetChannel(component));
  }
  bool HasChannels();
  void DestroyChannel(int component);

  // Set the local TransportDescription to be used by TransportChannels.
  // This should be called before ConnectChannels().
  bool SetLocalTransportDescription(const TransportDescription& description,
                                    ContentAction action);

  // Set the remote TransportDescription to be used by TransportChannels.
  bool SetRemoteTransportDescription(const TransportDescription& description,
                                     ContentAction action);

  // Tells all current and future channels to start connecting.  When the first
  // channel begins connecting, the following signal is raised.
  void ConnectChannels();
  sigslot::signal1<Transport*> SignalConnecting;

  // Resets all of the channels back to their initial state.  They are no
  // longer connecting.
  void ResetChannels();

  // Destroys every channel created so far.
  void DestroyAllChannels();

  bool GetStats(TransportStats* stats);

  // Before any stanza is sent, the manager will request signaling.  Once
  // signaling is available, the client should call OnSignalingReady.  Once
  // this occurs, the transport (or its channels) can send any waiting stanzas.
  // OnSignalingReady invokes OnTransportSignalingReady and then forwards this
  // signal to each channel.
  sigslot::signal1<Transport*> SignalRequestSignaling;
  void OnSignalingReady();

  // Handles sending of ready candidates and receiving of remote candidates.
  sigslot::signal2<Transport*,
                   const std::vector<Candidate>&> SignalCandidatesReady;

  sigslot::signal1<Transport*> SignalCandidatesAllocationDone;
  void OnRemoteCandidates(const std::vector<Candidate>& candidates);

  // If candidate is not acceptable, returns false and sets error.
  // Call this before calling OnRemoteCandidates.
  virtual bool VerifyCandidate(const Candidate& candidate,
                               std::string* error);

  // Signals when the best connection for a channel changes.
  sigslot::signal3<Transport*,
                   int,  // component
                   const Candidate&> SignalRouteChange;

  // A transport message has generated an transport-specific error.  The
  // stanza that caused the error is available in session_msg.  If false is
  // returned, the error is considered unrecoverable, and the session is
  // terminated.
  // TODO(juberti): Remove these obsolete functions once Session no longer
  // references them.
  virtual void OnTransportError(const buzz::XmlElement* error) {}
  sigslot::signal6<Transport*, const buzz::XmlElement*, const buzz::QName&,
                   const std::string&, const std::string&,
                   const buzz::XmlElement*>
      SignalTransportError;

  // Forwards the signal from TransportChannel to BaseSession.
  sigslot::signal0<> SignalRoleConflict;

 protected:
  // These are called by Create/DestroyChannel above in order to create or
  // destroy the appropriate type of channel.
  virtual TransportChannelImpl* CreateTransportChannel(int component) = 0;
  virtual void DestroyTransportChannel(TransportChannelImpl* channel) = 0;

  // Informs the subclass that we received the signaling ready message.
  virtual void OnTransportSignalingReady() {}

  // The current local transport description, for use by derived classes
  // when performing transport description negotiation.
  const TransportDescription* local_description() const {
    return local_description_.get();
  }

  // The current remote transport description, for use by derived classes
  // when performing transport description negotiation.
  const TransportDescription* remote_description() const {
    return remote_description_.get();
  }

  // Pushes down the transport parameters from the local description, such
  // as the ICE ufrag and pwd.
  // Derived classes can override, but must call the base as well.
  virtual bool ApplyLocalTransportDescription_w(TransportChannelImpl*
                                                channel);

  // Pushes down remote ice credentials from the remote description to the
  // transport channel.
  virtual bool ApplyRemoteTransportDescription_w(TransportChannelImpl* ch);

  // Negotiates the transport parameters based on the current local and remote
  // transport description, such at the version of ICE to use, and whether DTLS
  // should be activated.
  // Derived classes can negotiate their specific parameters here, but must call
  // the base as well.
  virtual bool NegotiateTransportDescription_w(ContentAction local_role);

  // Pushes down the transport parameters obtained via negotiation.
  // Derived classes can set their specific parameters here, but must call the
  // base as well.
  virtual void ApplyNegotiatedTransportDescription_w(
      TransportChannelImpl* channel);

 private:
  struct ChannelMapEntry {
    ChannelMapEntry() : impl_(NULL), candidates_allocated_(false), ref_(0) {}
    explicit ChannelMapEntry(TransportChannelImpl *impl)
        : impl_(impl),
          candidates_allocated_(false),
          ref_(0) {
    }

    void AddRef() { ++ref_; }
    void DecRef() {
      ASSERT(ref_ > 0);
      --ref_;
    }
    int ref() const { return ref_; }

    TransportChannelImpl* get() const { return impl_; }
    TransportChannelImpl* operator->() const  { return impl_; }
    void set_candidates_allocated(bool status) {
      candidates_allocated_ = status;
    }
    bool candidates_allocated() const { return candidates_allocated_; }

  private:
    TransportChannelImpl *impl_;
    bool candidates_allocated_;
    int ref_;
  };

  // Candidate component => ChannelMapEntry
  typedef std::map<int, ChannelMapEntry> ChannelMap;

  // Called when the state of a channel changes.
  void OnChannelReadableState(TransportChannel* channel);
  void OnChannelWritableState(TransportChannel* channel);

  // Called when a channel requests signaling.
  void OnChannelRequestSignaling(TransportChannelImpl* channel);

  // Called when a candidate is ready from remote peer.
  void OnRemoteCandidate(const Candidate& candidate);
  // Called when a candidate is ready from channel.
  void OnChannelCandidateReady(TransportChannelImpl* channel,
                               const Candidate& candidate);
  void OnChannelRouteChange(TransportChannel* channel,
                            const Candidate& remote_candidate);
  void OnChannelCandidatesAllocationDone(TransportChannelImpl* channel);
  // Called when there is ICE role change.
  void OnRoleConflict(TransportChannelImpl* channel);

  // Dispatches messages to the appropriate handler (below).
  void OnMessage(talk_base::Message* msg);

  // These are versions of the above methods that are called only on a
  // particular thread (s = signaling, w = worker).  The above methods post or
  // send a message to invoke this version.
  TransportChannelImpl* CreateChannel_w(int component);
  void DestroyChannel_w(int component);
  void ConnectChannels_w();
  void ResetChannels_w();
  void DestroyAllChannels_w();
  void OnRemoteCandidate_w(const Candidate& candidate);
  void OnChannelReadableState_s();
  void OnChannelWritableState_s();
  void OnChannelRequestSignaling_s(int component);
  void OnConnecting_s();
  void OnChannelRouteChange_s(const TransportChannel* channel,
                              const Candidate& remote_candidate);
  void OnChannelCandidatesAllocationDone_s();

  // Helper function that invokes the given function on every channel.
  typedef void (TransportChannelImpl::* TransportChannelFunc)();
  void CallChannels_w(TransportChannelFunc func);

  // Computes the OR of the channel's read or write state (argument picks).
  TransportState GetTransportState_s(bool read);

  void OnChannelCandidateReady_s();

  void SetRole_w(TransportRole role);
  void SetRemoteIceMode_w(IceMode mode);
  bool SetLocalTransportDescription_w(const TransportDescription& desc,
                                      ContentAction action);
  bool SetRemoteTransportDescription_w(const TransportDescription& desc,
                                       ContentAction action);
  bool GetStats_w(TransportStats* infos);

  talk_base::Thread* signaling_thread_;
  talk_base::Thread* worker_thread_;
  std::string content_name_;
  std::string type_;
  PortAllocator* allocator_;
  bool destroyed_;
  TransportState readable_;
  TransportState writable_;
  bool was_writable_;
  bool connect_requested_;
  TransportRole role_;
  uint64 tiebreaker_;
  TransportProtocol protocol_;
  IceMode remote_ice_mode_;
  talk_base::scoped_ptr<TransportDescription> local_description_;
  talk_base::scoped_ptr<TransportDescription> remote_description_;

  ChannelMap channels_;
  // Buffers the ready_candidates so that SignalCanidatesReady can
  // provide them in multiples.
  std::vector<Candidate> ready_candidates_;
  // Protects changes to channels and messages
  talk_base::CriticalSection crit_;

  DISALLOW_EVIL_CONSTRUCTORS(Transport);
};

// Extract a TransportProtocol from a TransportDescription.
TransportProtocol TransportProtocolFromDescription(
    const TransportDescription* desc);

}  // namespace cricket

#endif  // TALK_P2P_BASE_TRANSPORT_H_
