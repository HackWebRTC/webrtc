// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TALK_APP_WEBRTC_P2P_TRANSPORT_MANAGER_H_
#define TALK_APP_WEBRTC_P2P_TRANSPORT_MANAGER_H_

#include <string>

#include "talk/base/scoped_ptr.h"
#include "talk/base/sigslot.h"

namespace cricket {
class Candidate;
class P2PTransportChannel;
class PortAllocator;
class TransportChannel;
class TransportChannelImpl;
}

namespace talk_base {
class NetworkManager;
class PacketSocketFactory;
}

namespace webrtc {
class P2PTransportManager : public sigslot::has_slots<>{
 public:
  enum State {
    STATE_NONE = 0,
    STATE_WRITABLE = 1,
    STATE_READABLE = 2,
  };

  enum Protocol {
    PROTOCOL_UDP = 0,
    PROTOCOL_TCP = 1,
  };

  class EventHandler {
   public:
    virtual ~EventHandler() {}

    // Called for each local candidate.
    virtual void OnCandidateReady(const cricket::Candidate& candidate) = 0;

    // Called when readable of writable state of the stream changes.
    virtual void OnStateChange(State state) = 0;

    // Called when an error occures (e.g. TCP handshake
    // failed). P2PTransportManager object is not usable after that and
    // should be destroyed.
    virtual void OnError(int error) = 0;
  };

 public:
  // Create P2PTransportManager using specified NetworkManager and
  // PacketSocketFactory. Takes ownership of |network_manager| and
  // |socket_factory|.
  P2PTransportManager(cricket::PortAllocator* allocator);
  ~P2PTransportManager();

  bool Init(const std::string& name,
            Protocol protocol,
            const std::string& config,
            EventHandler* event_handler);
  bool AddRemoteCandidate(const cricket::Candidate& address);
  cricket::P2PTransportChannel* GetP2PChannel();

 private:

  void OnRequestSignaling();
  void OnCandidateReady(cricket::TransportChannelImpl* channel,
                        const cricket::Candidate& candidate);
  void OnReadableState(cricket::TransportChannel* channel);
  void OnWriteableState(cricket::TransportChannel* channel);

  std::string name_;
  EventHandler* event_handler_;
  State state_;

  cricket::PortAllocator* allocator_;
  talk_base::scoped_ptr<cricket::P2PTransportChannel> channel_;
};

}
#endif  // TALK_APP_WEBRTC_P2P_TRANSPORT_MANAGER_H_
