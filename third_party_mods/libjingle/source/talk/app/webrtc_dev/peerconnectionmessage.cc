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

scoped_refptr<PeerConnectionMessage> PeerConnectionMessage::Create(
    PeerConnectionMessageType type,
    cricket::SessionDescription* desc,
    const std::vector<cricket::Candidate>& candidates) {
  return new RefCountImpl<PeerConnectionMessage> (type, desc, candidates);
}

scoped_refptr<PeerConnectionMessage> PeerConnectionMessage::Create(
    const std::string& message) {
  scoped_refptr<PeerConnectionMessage>pc_message(new
      RefCountImpl<PeerConnectionMessage> ());
  if (!pc_message->Deserialize(message))
    return NULL;
  return pc_message;
}

scoped_refptr<PeerConnectionMessage> PeerConnectionMessage::CreateErrorMessage(
    ErrorCode error) {
  return new RefCountImpl<PeerConnectionMessage> (error);
}

PeerConnectionMessage::PeerConnectionMessage(
    PeerConnectionMessageType type,
    cricket::SessionDescription* desc,
    const std::vector<cricket::Candidate>& candidates)
    : type_(type),
      error_code_(kNoError),
      desc_(desc),
      candidates_(candidates) {
}

PeerConnectionMessage::PeerConnectionMessage()
    : type_(kOffer),
      error_code_(kNoError),
      desc_(new cricket::SessionDescription()) {
}

PeerConnectionMessage::PeerConnectionMessage(ErrorCode error)
    : type_(kError),
      desc_(NULL),
      error_code_(error) {
}

std::string PeerConnectionMessage::Serialize() {
  return JsonSerialize(type_, error_code_, desc_.get(), candidates_);
}

bool PeerConnectionMessage::Deserialize(std::string message) {
  return JsonDeserialize(&type_, &error_code_, desc_.get(),
      &candidates_, message);
}

}  // namespace webrtc
