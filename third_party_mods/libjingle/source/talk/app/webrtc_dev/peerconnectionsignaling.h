/*
 * libjingle
 * Copyright 2011, Google Inc.
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

// This file contains classes used for handling signaling between
// two PeerConnections.

#ifndef TALK_APP_WEBRTC_PEERCONNECTIONSIGNALING_H_
#define TALK_APP_WEBRTC_PEERCONNECTIONSIGNALING_H_

#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "talk/app/webrtc_dev/mediastreamproxy.h"
#include "talk/app/webrtc_dev/peerconnection.h"
#include "talk/app/webrtc_dev/peerconnectionmessage.h"
#include "talk/app/webrtc_dev/ref_count.h"
#include "talk/app/webrtc_dev/scoped_refptr.h"
#include "talk/app/webrtc_dev/sessiondescriptionprovider.h"
#include "talk/app/webrtc_dev/webrtcsessionobserver.h"
#include "talk/base/basictypes.h"
#include "talk/base/messagehandler.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/thread.h"
#include "talk/session/phone/mediasession.h"
#include "talk/p2p/base/sessiondescription.h"

namespace cricket {
class ChannelManager;
class Candidate;
typedef std::vector<Candidate> Candidates;
}

namespace webrtc {

// PeerConnectionSignaling is a class responsible for handling signaling
// between PeerConnection objects.
// It creates remote MediaStream objects when the remote peer signals it wants
// to send a new MediaStream.
// It changes the state of local MediaStreams and tracks
// when a remote peer is ready to receive media.
// Call Initialize when local Candidates are ready.
// Call CreateOffer to negotiate new local streams to send.
// Call ProcessSignalingMessage when a new PeerConnectionMessage have been
// received from the remote peer.
// Before PeerConnectionSignaling can process an answer or create an offer,
// Initialize have to be called. The last request to create an offer or process
// an answer will be processed after Initialize have been called.
class PeerConnectionSignaling : public WebRtcSessionObserver,
                                public talk_base::MessageHandler {
 public:
  enum State {
    // Awaiting the local candidates.
    kInitializing,
    // Ready to sent new offer or receive a new offer.
    kIdle,
    // We have sent an offer and expect an answer, or we want to update
    // our own offer.
    kWaitingForAnswer,
    // While waiting for an answer to our offer we received an offer from
    // the remote peer.
    kGlare
  };

  PeerConnectionSignaling(talk_base::Thread* signaling_thread,
                          SessionDescriptionProvider* provider);
  ~PeerConnectionSignaling();

  // Process a received offer/answer from the remote peer.
  void ProcessSignalingMessage(const std::string& message,
                               StreamCollectionInterface* local_streams);

  // Creates an offer containing all tracks in local_streams.
  // When the offer is ready it is signaled by SignalNewPeerConnectionMessage.
  // When the remote peer is ready to receive media on a stream , the state of
  // the local stream will change to kAlive.
  void CreateOffer(StreamCollectionInterface* local_streams);

  // Returns the current state.
  State GetState();

  // New PeerConnectionMessage with an SDP offer/answer is ready to be sent.
  // The listener to this signal is expected to serialize and send the
  // PeerConnectionMessage to the remote peer.
  sigslot::signal1<const std::string&> SignalNewPeerConnectionMessage;

  // A new remote stream have been discovered.
  sigslot::signal1<MediaStreamInterface*> SignalRemoteStreamAdded;

  // Remote stream is no longer available.
  sigslot::signal1<MediaStreamInterface*> SignalRemoteStreamRemoved;

  // Remote PeerConnection sent an error message.
  sigslot::signal1<PeerConnectionMessage::ErrorCode> SignalErrorMessageReceived;

  // Implements WebRtcSessionObserver
  virtual void OnCandidatesReady(const cricket::Candidates& candidates);
  // Implements talk_base::MessageHandler.
  virtual void OnMessage(talk_base::Message* msg);

 private:
  void CreateOffer_s();
  void CreateAnswer_s();

  void InitMediaSessionOptions(cricket::MediaSessionOptions* options,
                               StreamCollectionInterface* local_streams);

  void UpdateRemoteStreams(const cricket::SessionDescription* remote_desc);
  void UpdateSendingLocalStreams(
      const cricket::SessionDescription* answer_desc,
      StreamCollectionInterface* negotiated_streams);

  typedef std::list<talk_base::scoped_refptr<StreamCollectionInterface> >
      StreamCollectionList;
  StreamCollectionList queued_offers_;

  typedef std::pair<PeerConnectionMessage*,
                    talk_base::scoped_refptr<StreamCollectionInterface> >
      RemoteOfferPair;
  RemoteOfferPair queued_received_offer_;

  talk_base::Thread* signaling_thread_;
  SessionDescriptionProvider* provider_;
  State state_;
  uint32 ssrc_counter_;

  typedef std::map<std::string, talk_base::scoped_refptr<MediaStreamProxy> >
      RemoteStreamMap;
  RemoteStreamMap remote_streams_;
  typedef std::map<std::string, talk_base::scoped_refptr<MediaStreamInterface> >
      LocalStreamMap;
  LocalStreamMap local_streams_;
  cricket::Candidates candidates_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_PEERCONNECTIONSIGNALING_H_
