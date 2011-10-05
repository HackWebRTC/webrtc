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

#include "talk/app/webrtc_dev/peerconnectionmessage.h"

#include <string>
#include <vector>

#include "talk/app/webrtc_dev/webrtcjson.h"

namespace webrtc {

PeerConnectionMessage* PeerConnectionMessage::Create(
    PeerConnectionMessageType type,
    const cricket::SessionDescription* desc,
    const std::vector<cricket::Candidate>& candidates) {
  return new PeerConnectionMessage(type, desc, candidates);
}

PeerConnectionMessage* PeerConnectionMessage::Create(
    const std::string& message) {
  PeerConnectionMessage* pc_message(new PeerConnectionMessage());
  if (!pc_message->Deserialize(message))
    return NULL;
  return pc_message;
}

PeerConnectionMessage* PeerConnectionMessage::CreateErrorMessage(
    ErrorCode error) {
  return new PeerConnectionMessage(error);
}

PeerConnectionMessage::PeerConnectionMessage(
    PeerConnectionMessageType type,
    const cricket::SessionDescription* desc,
    const std::vector<cricket::Candidate>& candidates)
    : type_(type),
      error_code_(kNoError),
      desc_(desc),
      candidates_(candidates) {
}

PeerConnectionMessage::PeerConnectionMessage()
    : type_(kOffer),
      error_code_(kNoError),
      desc_(NULL) {
}

PeerConnectionMessage::PeerConnectionMessage(ErrorCode error)
    : type_(kError),
      desc_(NULL),
      error_code_(error) {
}

std::string PeerConnectionMessage::Serialize() {
  return JsonSerialize(type_, error_code_, desc_, candidates_);
}

bool PeerConnectionMessage::Deserialize(std::string message) {
  cricket::SessionDescription* desc(new cricket::SessionDescription());
  bool result = JsonDeserialize(&type_, &error_code_, desc,
                                &candidates_, message);
  if(!result) {
    delete desc;
    desc = NULL;
  }
  desc_ = desc;
  return result;
}

}  // namespace webrtc
