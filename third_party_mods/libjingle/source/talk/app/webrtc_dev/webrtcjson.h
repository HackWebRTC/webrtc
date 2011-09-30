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

#ifndef TALK_APP_WEBRTC_WEBRTCJSON_H_
#define TALK_APP_WEBRTC_WEBRTCJSON_H_

#include <string>

#ifdef WEBRTC_RELATIVE_PATH
#include "json/json.h"
#else
#include "third_party/jsoncpp/json.h"
#endif
#include "talk/app/webrtc_dev/peerconnectionmessage.h"
#include "talk/p2p/base/candidate.h"

namespace cricket {
class SessionDescription;
}

namespace webrtc {

bool JsonSerialize(
    const webrtc::PeerConnectionMessage::PeerConnectionMessageType type,
    int error_code,
    const cricket::SessionDescription* sdp,
    const std::vector<cricket::Candidate>& candidates,
    std::string* signaling_message);

bool JsonDeserialize(
    webrtc::PeerConnectionMessage::PeerConnectionMessageType* type,
    webrtc::PeerConnectionMessage::ErrorCode* error_code,
    cricket::SessionDescription* sdp,
    std::vector<cricket::Candidate>* candidates,
    const std::string& signaling_message);
}

#endif  // TALK_APP_WEBRTC_WEBRTCJSON_H_
