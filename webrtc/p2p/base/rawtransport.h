/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_RAWTRANSPORT_H_
#define WEBRTC_P2P_BASE_RAWTRANSPORT_H_

#include <string>
#include "webrtc/p2p/base/transport.h"

#if defined(FEATURE_ENABLE_PSTN)
namespace cricket {

// Implements a transport that only sends raw packets, no STUN.  As a result,
// it cannot do pings to determine connectivity, so it only uses a single port
// that it thinks will work.
class RawTransport : public Transport, public TransportParser {
 public:
  RawTransport(rtc::Thread* signaling_thread,
               rtc::Thread* worker_thread,
               const std::string& content_name,
               PortAllocator* allocator);
  virtual ~RawTransport();

  virtual bool ParseCandidates(SignalingProtocol protocol,
                               const buzz::XmlElement* elem,
                               const CandidateTranslator* translator,
                               Candidates* candidates,
                               ParseError* error);
  virtual bool WriteCandidates(SignalingProtocol protocol,
                               const Candidates& candidates,
                               const CandidateTranslator* translator,
                               XmlElements* candidate_elems,
                               WriteError* error);

 protected:
  // Creates and destroys raw channels.
  virtual TransportChannelImpl* CreateTransportChannel(int component);
  virtual void DestroyTransportChannel(TransportChannelImpl* channel);

 private:
  // Parses the given element, which should describe the address to use for a
  // given channel.  This will return false and signal an error if the address
  // or channel name is bad.
  bool ParseRawAddress(const buzz::XmlElement* elem,
                       rtc::SocketAddress* addr,
                       ParseError* error);

  friend class RawTransportChannel;  // For ParseAddress.

  DISALLOW_EVIL_CONSTRUCTORS(RawTransport);
};

}  // namespace cricket

#endif  // defined(FEATURE_ENABLE_PSTN)

#endif  // WEBRTC_P2P_BASE_RAWTRANSPORT_H_
