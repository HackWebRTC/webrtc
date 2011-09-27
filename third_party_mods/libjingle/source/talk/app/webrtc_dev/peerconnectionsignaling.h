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

#include "talk/app/webrtc_dev/mediastreamimpl.h"
#include "talk/app/webrtc_dev/peerconnection.h"
#include "talk/app/webrtc_dev/ref_count.h"
#include "talk/app/webrtc_dev/scoped_refptr.h"
#include "talk/base/basictypes.h"
#include "talk/base/messagehandler.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/thread.h"
#include "talk/session/phone/mediasession.h"
#include "talk/p2p/base/sessiondescription.h"

namespace cricket {
class ChannelManager;
}

namespace webrtc {

// PeerConnectionMessage represent an SDP offer or an answer.
// Instances of this class can be serialized / deserialized and are used for
// signaling between PeerConnection objects.
// Each instance has a type, a sequence number and a session description.
class PeerConnectionMessage : public RefCount {
 public:
  enum PeerConnectionMessageType {
    kOffer,
    kAnswer,
    kError
  };

  enum ErrorCode {
    kNoError = 0,
    kWrongState = 10,  // Offer received when Answer was expected.
    kParseError = 20,  // Can't parse / process offer.
    kOfferNotAcceptable = 30,  // The offer have been rejected.
    kMessageNotDeliverable = 40  // The signaling channel is broken.
  };

  static scoped_refptr<PeerConnectionMessage> Create(
      PeerConnectionMessageType type,
      const cricket::SessionDescription* desc);

  static scoped_refptr<PeerConnectionMessage> CreateErrorMessage(
      ErrorCode error);

  PeerConnectionMessageType type() {return type_;}
  ErrorCode error() {return error_code_;}
  const cricket::SessionDescription* desc() {return desc_.get();}

  // TODO(perkj): Add functions for serializing and deserializing this class.

 protected:
  PeerConnectionMessage(PeerConnectionMessageType type,
                        const cricket::SessionDescription* desc);
  explicit PeerConnectionMessage(ErrorCode error);

 private:
  PeerConnectionMessageType type_;
  ErrorCode error_code_;
  talk_base::scoped_ptr<const cricket::SessionDescription> desc_;
};

// PeerConnectionSignaling is a class responsible for handling signaling
// between PeerConnection objects.
// It creates remote MediaStream objects when the remote peer signals it wants
// to send a new MediaStream.
// It changes the state of local MediaStreams and tracks
// when a remote peer is ready to receive media.
// Call CreateOffer to negotiate new local streams to send.
// Call ProcessSignalingMessage when a new PeerConnectionMessage have been
// received from the remote peer.
class PeerConnectionSignaling : public talk_base::MessageHandler {
 public:
  enum State {
    // Ready to sent new offer or receive a new offer.
    kIdle,
    // We have sent an offer and expect an answer, or we want to update
    // our own offer.
    kWaitingForAnswer,
    // While waiting for an answer to our offer we received an offer from
    // the remote peer.
    kGlare
  };

  explicit PeerConnectionSignaling(cricket::ChannelManager* channel_manager);
  ~PeerConnectionSignaling();

  // Process a received offer/answer from the remote peer.
  void ProcessSignalingMessage(PeerConnectionMessage* message,
                               StreamCollection* local_streams);

  // Creates an offer containing all tracks in local_streams.
  // When the offer is ready it is signaled by SignalNewPeerConnectionMessage.
  // When the remote peer is ready to receive media on a stream , the state of
  // the local stream will change to kAlive.
  void CreateOffer(StreamCollection* local_streams);

  // Returns the current state.
  State GetState();

  // New PeerConnectionMessage with an SDP offer/answer is ready to be sent.
  // The listener to this signal is expected to serialize and send the
  // PeerConnectionMessage to the remote peer.
  sigslot::signal1<PeerConnectionMessage*> SignalNewPeerConnectionMessage;

  // A new remote stream have been discovered.
  sigslot::signal1<MediaStream*> SignalRemoteStreamAdded;

  // Remote stream is no longer available.
  sigslot::signal1<MediaStream*> SignalRemoteStreamRemoved;

  // Remote PeerConnection sent an error message.
  sigslot::signal1<PeerConnectionMessage::ErrorCode> SignalErrorMessageReceived;

 private:
  // Implement talk_base::MessageHandler.
  virtual void OnMessage(talk_base::Message* msg);
  void CreateOffer_s();
  void GenerateAnswer(PeerConnectionMessage* message,
                      StreamCollection* local_streams);

  void InitMediaSessionOptions(cricket::MediaSessionOptions* options,
                                  StreamCollection* local_streams);

  void UpdateRemoteStreams(const cricket::SessionDescription* remote_desc);
  void UpdateSendingLocalStreams(
      const cricket::SessionDescription* answer_desc,
      StreamCollection* negotiated_streams);

  typedef std::list<scoped_refptr<StreamCollection> > StreamCollectionList;
  StreamCollectionList queued_offers_;

  talk_base::Thread* signaling_thread_;
  State state_;
  uint32 ssrc_counter_;

  typedef std::map<std::string, scoped_refptr<MediaStreamImpl> >
      RemoteStreamMap;
  RemoteStreamMap remote_streams_;
  typedef std::map<std::string, scoped_refptr<MediaStream> >
      LocalStreamMap;
  LocalStreamMap local_streams_;
  cricket::MediaSessionDescriptionFactory session_description_factory_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_PEERCONNECTIONSIGNALING_H_
