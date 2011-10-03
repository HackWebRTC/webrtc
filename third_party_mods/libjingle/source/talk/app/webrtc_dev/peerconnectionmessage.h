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

#ifndef TALK_APP_WEBRTC_DEV_PEERCONNECTIONMESSAGE_H_
#define TALK_APP_WEBRTC_DEV_PEERCONNECTIONMESSAGE_H_

#include <string>
#include <vector>

#include "talk/app/webrtc_dev/ref_count.h"
#include "talk/app/webrtc_dev/scoped_refptr.h"
#include "talk/base/basictypes.h"
#include "talk/base/scoped_ptr.h"
#include "talk/session/phone/mediasession.h"
#include "talk/p2p/base/sessiondescription.h"

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
      cricket::SessionDescription* desc,
      const std::vector<cricket::Candidate>& candidates);

  static scoped_refptr<PeerConnectionMessage> Create(
      const std::string& message);

  static scoped_refptr<PeerConnectionMessage> CreateErrorMessage(
      ErrorCode error);

  PeerConnectionMessageType type() {return type_;}
  ErrorCode error() {return error_code_;}
  const cricket::SessionDescription* desc() {return desc_.get();}

  bool Serialize(std::string* message);
  std::vector<cricket::Candidate>& candidates() { return candidates_; }

 protected:
  PeerConnectionMessage(PeerConnectionMessageType type,
                        cricket::SessionDescription* desc,
                        const std::vector<cricket::Candidate>& candidates);
  PeerConnectionMessage();
  explicit PeerConnectionMessage(ErrorCode error);

  bool Deserialize(std::string message);

 private:
  PeerConnectionMessageType type_;
  ErrorCode error_code_;
  talk_base::scoped_ptr<cricket::SessionDescription> desc_;
  std::vector<cricket::Candidate> candidates_;

};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_DEV_PEERCONNECTIONMESSAGE_H_
