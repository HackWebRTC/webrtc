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

#include "talk/app/webrtc_dev/peerconnectionmanagerimpl.h"

#include "talk/app/webrtc_dev/peerconnectionimpl.h"
#include "talk/app/webrtc_dev/webrtc_devicemanager.h"
#include "talk/base/basicpacketsocketfactory.h"
#include "talk/base/thread.h"
#include "talk/session/phone/channelmanager.h"
#include "talk/session/phone/webrtcmediaengine.h"

#ifdef WEBRTC_RELATIVE_PATH
#include "modules/audio_device/main/interface/audio_device.h"
#else
#include "third_party/webrtc/files/include/audio_device.h"
#endif


namespace webrtc {

scoped_refptr<PcNetworkManager> PcNetworkManager::Create(
    talk_base::NetworkManager* network_manager) {
  RefCountImpl<PcNetworkManager>* implementation =
       new RefCountImpl<PcNetworkManager>(network_manager);
  return implementation;
}

PcNetworkManager::PcNetworkManager(talk_base::NetworkManager* network_manager)
    : network_manager_(network_manager) {
}

talk_base::NetworkManager* PcNetworkManager::network_manager() const {
  return network_manager_;
}

PcNetworkManager::~PcNetworkManager() {
  delete network_manager_;
}

scoped_refptr<PcPacketSocketFactory> PcPacketSocketFactory::Create(
    talk_base::PacketSocketFactory* socket_factory) {
  RefCountImpl<PcPacketSocketFactory>* implementation =
       new RefCountImpl<PcPacketSocketFactory>(socket_factory);
  return implementation;
}

PcPacketSocketFactory::PcPacketSocketFactory(
    talk_base::PacketSocketFactory* socket_factory)
    : socket_factory_(socket_factory) {
}

PcPacketSocketFactory::~PcPacketSocketFactory() {
  delete socket_factory_;
}

talk_base::PacketSocketFactory* PcPacketSocketFactory::socket_factory() const {
  return socket_factory_;
}

scoped_refptr<PeerConnectionManager> PeerConnectionManager::Create() {
  RefCountImpl<PeerConnectionManagerImpl>* pc_manager =
      new RefCountImpl<PeerConnectionManagerImpl>();
  if (!pc_manager->Initialize()) {
    delete pc_manager;
    pc_manager = NULL;
  }
  return pc_manager;
}

scoped_refptr<PeerConnectionManager> PeerConnectionManager::Create(
    talk_base::Thread* worker_thread,
    PcNetworkManager* network_manager,
    PcPacketSocketFactory* socket_factory,
    AudioDeviceModule* default_adm) {
  RefCountImpl<PeerConnectionManagerImpl>* pc_manager =
      new RefCountImpl<PeerConnectionManagerImpl>(worker_thread,
                                                  network_manager,
                                                  socket_factory,
                                                  default_adm);
  if (!pc_manager->Initialize()) {
    delete pc_manager;
    pc_manager = NULL;
  }
  return pc_manager;
}

PeerConnectionManagerImpl::PeerConnectionManagerImpl()
    : worker_thread_(new talk_base::Thread),
      network_manager_(PcNetworkManager::Create(
          new talk_base::BasicNetworkManager())),
      socket_factory_(PcPacketSocketFactory::Create(
          new talk_base::BasicPacketSocketFactory)) {
  worker_thread_ptr_ = worker_thread_.get();
}

PeerConnectionManagerImpl::PeerConnectionManagerImpl(
    talk_base::Thread* worker_thread,
    PcNetworkManager* network_manager,
    PcPacketSocketFactory* socket_factory,
    AudioDeviceModule* default_adm)
    : worker_thread_ptr_(worker_thread),
      network_manager_(network_manager),
      socket_factory_(socket_factory),
      default_adm_(default_adm) {
  ASSERT(worker_thread);
  ASSERT(network_manager->network_manager());
  ASSERT(socket_factory->socket_factory());
  ASSERT(default_adm);
}

PeerConnectionManagerImpl::~PeerConnectionManagerImpl() {
}

bool PeerConnectionManagerImpl::Initialize() {
  if (worker_thread_.get() && !worker_thread_->Start())
    return false;
  cricket::DeviceManager* device_manager(new WebRtcDeviceManager());
  cricket::WebRtcMediaEngine* webrtc_media_engine = NULL;

  // TODO(perkj):  Need to make sure only one VoE is created inside
  // WebRtcMediaEngine.
  webrtc_media_engine = new cricket::WebRtcMediaEngine(
      default_adm_.get(),
      NULL,   // No secondary adm.
      NULL);  // No vcm available.

  channel_manager_.reset(new cricket::ChannelManager(
      webrtc_media_engine, device_manager, worker_thread_ptr_));
  if (!channel_manager_->Init()) {
    return false;
  }
  return true;
}

scoped_refptr<PeerConnection> PeerConnectionManagerImpl::CreatePeerConnection(
    const std::string& configuration) {
  RefCountImpl<PeerConnectionImpl>* pc =
      new RefCountImpl<PeerConnectionImpl>(channel_manager_.get(),
                                           worker_thread_ptr_,
                                           network_manager_,
                                           socket_factory_);
  if (!pc->Initialize(configuration)) {
    delete pc;
    pc = NULL;
  }
  return pc;
}

}  // namespace webrtc
