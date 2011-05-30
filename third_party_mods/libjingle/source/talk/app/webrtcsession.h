/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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

#ifndef TALK_APP_WEBRTC_WEBRTCSESSION_H_
#define TALK_APP_WEBRTC_WEBRTCSESSION_H_

#include "talk/base/logging.h"
#include "talk/p2p/base/constants.h"
#include "talk/p2p/base/session.h"

namespace cricket {
class PortAllocator;
}

namespace webrtc {
class PeerConnection;

class WebRTCSession: public cricket::BaseSession {
 public:
  WebRTCSession(const std::string& id, const std::string& direction,
                cricket::PortAllocator* allocator,
                PeerConnection* connection,
                talk_base::Thread* signaling_thread)
      : BaseSession(signaling_thread),
        signaling_thread_(signaling_thread),
        id_(id),
        incoming_(direction == kIncomingDirection),
        port_allocator_(allocator),
        connection_(connection) {
    BaseSession::sid_ = id;
  }

  virtual ~WebRTCSession() {
  }

  virtual bool Initiate() = 0;

  const std::string& id() const { return id_; }
  //const std::string& type() const { return type_; }
  bool incoming() const { return incoming_; }
  cricket::PortAllocator* port_allocator() const { return port_allocator_; }

//  static const std::string kAudioType;
//  static const std::string kVideoType;
  static const std::string kIncomingDirection;
  static const std::string kOutgoingDirection;
//  static const std::string kTestType;
  PeerConnection* connection() const { return connection_; }

 protected:
  //methods from cricket::BaseSession
  virtual bool Accept(const cricket::SessionDescription* sdesc) {
    return true;
  }
  virtual bool Reject(const std::string& reason) {
    return true;
  }
  virtual bool TerminateWithReason(const std::string& reason) {
    return true;
  }

 protected:
  talk_base::Thread* signaling_thread_;

 private:
  std::string id_;
  //std::string type_;
  bool incoming_;
  cricket::PortAllocator* port_allocator_;
  PeerConnection* connection_;
};

} // namespace webrtc


#endif /* TALK_APP_WEBRTC_WEBRTCSESSION_H_ */
