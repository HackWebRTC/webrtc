/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/pc/ortcfactory.h"

#include <string>
#include <utility>  // For std::move.

#include "webrtc/base/bind.h"
#include "webrtc/base/asyncpacketsocket.h"
#include "webrtc/p2p/base/basicpacketsocketfactory.h"
#include "webrtc/p2p/base/udptransport.h"

namespace webrtc {

// static
std::unique_ptr<OrtcFactoryInterface> OrtcFactoryInterface::Create(
    rtc::Thread* network_thread,
    rtc::Thread* signaling_thread,
    rtc::NetworkManager* network_manager,
    rtc::PacketSocketFactory* socket_factory) {
  // Hop to signaling thread if needed.
  if (signaling_thread && !signaling_thread->IsCurrent()) {
    // The template parameters are necessary because there are two
    // OrtcFactoryInterface::Create methods, so the types can't be derived from
    // just the function pointer.
    return signaling_thread->Invoke<std::unique_ptr<OrtcFactoryInterface>>(
        RTC_FROM_HERE,
        rtc::Bind<std::unique_ptr<OrtcFactoryInterface>, rtc::Thread*,
                  rtc::Thread*, rtc::NetworkManager*,
                  rtc::PacketSocketFactory*>(&OrtcFactoryInterface::Create,
                                             network_thread, signaling_thread,
                                             network_manager, socket_factory));
  }
  OrtcFactory* new_factory =
      new OrtcFactory(network_thread, signaling_thread,
                      network_manager, socket_factory);
  // Return a proxy so that any calls on the returned object (including
  // destructor) happen on the signaling thread.
  return OrtcFactoryProxy::Create(new_factory->signaling_thread(),
                                  new_factory->network_thread(), new_factory);
}

OrtcFactory::OrtcFactory(rtc::Thread* network_thread,
                         rtc::Thread* signaling_thread,
                         rtc::NetworkManager* network_manager,
                         rtc::PacketSocketFactory* socket_factory)
    : network_thread_(network_thread),
      signaling_thread_(signaling_thread),
      network_manager_(network_manager),
      socket_factory_(socket_factory) {
  if (!network_thread_) {
    owned_network_thread_ = rtc::Thread::CreateWithSocketServer();
    owned_network_thread_->Start();
    network_thread_ = owned_network_thread_.get();
  }

  // The worker thread is created internally because it's an implementation
  // detail, and consumers of the API don't need to really know about it.
  owned_worker_thread_ = rtc::Thread::Create();
  owned_worker_thread_->Start();

  if (signaling_thread_) {
    RTC_DCHECK_RUN_ON(signaling_thread_);
  } else {
    signaling_thread_ = rtc::Thread::Current();
    if (!signaling_thread_) {
      // If this thread isn't already wrapped by an rtc::Thread, create a
      // wrapper and own it in this class.
      signaling_thread_ = rtc::ThreadManager::Instance()->WrapCurrentThread();
      wraps_signaling_thread_ = true;
    }
  }
  if (!network_manager_) {
    owned_network_manager_.reset(new rtc::BasicNetworkManager());
    network_manager_ = owned_network_manager_.get();
  }
  if (!socket_factory_) {
    owned_socket_factory_.reset(
        new rtc::BasicPacketSocketFactory(network_thread_));
    socket_factory_ = owned_socket_factory_.get();
  }
}

OrtcFactory::~OrtcFactory() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  if (wraps_signaling_thread_) {
    rtc::ThreadManager::Instance()->UnwrapCurrentThread();
  }
}

std::unique_ptr<UdpTransportInterface> OrtcFactory::CreateUdpTransport(
    int family,
    uint16_t min_port,
    uint16_t max_port) {
  if (!network_thread_->IsCurrent()) {
    RTC_DCHECK_RUN_ON(signaling_thread_);
    return network_thread_->Invoke<std::unique_ptr<UdpTransportInterface>>(
        RTC_FROM_HERE, rtc::Bind(&OrtcFactory::CreateUdpTransport, this, family,
                                 min_port, max_port));
  }
  std::unique_ptr<rtc::AsyncPacketSocket> socket(
      socket_factory_->CreateUdpSocket(
          rtc::SocketAddress(rtc::GetAnyIP(family), 0), min_port, max_port));
  if (!socket) {
    LOG(LS_WARNING) << "Local socket allocation failure.";
    return nullptr;
  }
  LOG(LS_INFO) << "Created UDP socket with address "
               << socket->GetLocalAddress().ToSensitiveString() << ".";
  // Use proxy so that calls to the returned object are invoked on the network
  // thread.
  return UdpTransportProxy::Create(
      signaling_thread_, network_thread_,
      new cricket::UdpTransport(std::string(), std::move(socket)));
}

}  // namespace webrtc
