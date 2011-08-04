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

#ifndef TALK_APP_WEBRTC_PEERCONNECTION_H_
#define TALK_APP_WEBRTC_PEERCONNECTION_H_

#include <string>

#include "talk/app/webrtc/stream_dev.h"

namespace cricket {
class PortAllocator;
class DeviceManager;
}

namespace talk_base {
class Thread;
}

namespace webrtc {

/////////////////////////////////////////////
class PeerConnectionObserver {
 public:
  enum Readiness {
    kNegotiating,
    kActive,
  };

  virtual void OnError() = 0;

  virtual void OnMessage(const std::string& msg) = 0;

  // serialized signaling message
  // First message will be the initial offer.
  virtual void OnSignalingMessage(const std::string& msg) = 0;

  virtual void OnStateChange(Readiness state) = 0;

  // Triggered when media is received on a new stream from remote peer.
  // The label is unique for a certain peer_id.
  virtual void OnAddStream(scoped_refptr<RemoteStream> stream) = 0;

  // Triggered when a remote peer close a stream.
  virtual void OnRemoveStream(scoped_refptr<RemoteStream> stream) = 0;

 protected:
  // Dtor protected as objects shouldn't be deleted via this interface.
  ~PeerConnectionObserver() {}
};

class StreamCollection : public RefCount {
 public:
  virtual size_t count() = 0;
  virtual MediaStream* at(size_t index) = 0;
};

class PeerConnection {
 public:
  // Start Negotiation. Negotiation is based on if
  // SignalingMessage and AddStream have been called prior to this function.
  virtual bool StartNegotiation() = 0;

  // SignalingMessage in json format
  virtual bool SignalingMessage(const std::string& msg) = 0;

  // Sends the msg over a data stream.
  virtual bool Send(const std::string& msg) = 0;

  // Accessor methods to active local streams.
  virtual scoped_refptr<StreamCollection> local_streams() = 0;

  // Accessor methods to remote streams.
  virtual scoped_refptr<StreamCollection> remote_streams() = 0;

  // Add a new local stream.
  virtual void AddStream(LocalStream* stream) = 0;

  // Remove a local stream and stop sending it.
  virtual void RemoveStream(LocalStream* stream) = 0;

  virtual ~PeerConnection(){};

};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_PEERCONNECTION_H_
