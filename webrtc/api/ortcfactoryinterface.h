/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_ORTCFACTORYINTERFACE_H_
#define WEBRTC_API_ORTCFACTORYINTERFACE_H_

#include <memory>

#include "webrtc/api/udptransportinterface.h"
#include "webrtc/base/network.h"
#include "webrtc/base/thread.h"
#include "webrtc/p2p/base/packetsocketfactory.h"

namespace webrtc {

// WARNING: This is experimental/under development, so use at your own risk; no
// guarantee about API stability is guaranteed here yet.
//
// This class is the ORTC analog of PeerConnectionFactory. It acts as a factory
// for ORTC objects that can be connected to each other.
//
// Some of these objects may not be represented by the ORTC specification, but
// follow the same general principles.
//
// On object lifetimes: The factory must not be destroyed before destroying the
// objects it created, and the objects passed into the factory must not be
// destroyed before destroying the factory.
class OrtcFactoryInterface {
 public:
  // |network_thread| is the thread on which packets are sent and received.
  // If null, a new rtc::Thread with a default socket server is created.
  //
  // |signaling_thread| is used for callbacks to the consumer of the API. If
  // null, the current thread will be used, which assumes that the API consumer
  // is running a message loop on this thread (either using an existing
  // rtc::Thread, or by calling rtc::Thread::Current()->ProcessMessages).
  //
  // |network_manager| is used to determine which network interfaces are
  // available. This is used for ICE, for example. If null, a default
  // implementation will be used. Only accessed on |network_thread|.
  //
  // |socket_factory| is used (on the network thread) for creating sockets. If
  // it's null, a default implementation will be used, which assumes
  // |network_thread| is a normal rtc::Thread.
  //
  // Note that the OrtcFactoryInterface does not take ownership of any of the
  // objects
  // passed in, and as previously stated, these objects can't be destroyed
  // before the factory is.
  static std::unique_ptr<OrtcFactoryInterface> Create(
      rtc::Thread* network_thread,
      rtc::Thread* signaling_thread,
      rtc::NetworkManager* network_manager,
      rtc::PacketSocketFactory* socket_factory);
  // Constructor for convenience which uses default implementations of
  // everything (though does still require that the current thread runs a
  // message loop; see above).
  static std::unique_ptr<OrtcFactoryInterface> Create() {
    return Create(nullptr, nullptr, nullptr, nullptr);
  }

  virtual ~OrtcFactoryInterface() {}

  virtual std::unique_ptr<UdpTransportInterface>
  CreateUdpTransport(int family, uint16_t min_port, uint16_t max_port) = 0;
  // Method for convenience that has no port range restrictions.
  std::unique_ptr<UdpTransportInterface> CreateUdpTransport(int family) {
    return CreateUdpTransport(family, 0, 0);
  }
};

}  // namespace webrtc

#endif  // WEBRTC_API_ORTCFACTORYINTERFACE_H_
