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

#include "talk/app/webrtc_dev/peerconnectionmanager.h"

#include "talk/app/webrtc_dev/peerconnection_impl_dev.h"
#include "talk/base/logging.h"
#include "talk/base/thread.h"
#include "talk/session/phone/channelmanager.h"

namespace webrtc {

PeerConnectionManager* PeerConnectionManager::Create(
    cricket::MediaEngine* media_engine,
    cricket::DeviceManager* device_manager,
    cricket::PortAllocator* port_allocator,
    talk_base::Thread* worker_thread) {
  PeerConnectionManager* pc_manager = new PeerConnectionManager();
  if (!pc_manager->Initialize(media_engine, device_manager, port_allocator,
                              worker_thread)) {
    delete pc_manager;
    pc_manager = NULL;
  }
  return pc_manager;
}

PeerConnectionManager* PeerConnectionManager::Create(
    cricket::PortAllocator* port_allocator,
    talk_base::Thread* worker_thread) {
  PeerConnectionManager* pc_manager = new PeerConnectionManager();
  if (!pc_manager->Initialize(port_allocator, worker_thread)) {
    delete pc_manager;
    pc_manager = NULL;
  }
  return pc_manager;
}

bool PeerConnectionManager::Initialize(cricket::MediaEngine* media_engine,
                                       cricket::DeviceManager* device_manager,
                                       cricket::PortAllocator* port_allocator,
                                       talk_base::Thread* worker_thread) {
  channel_manager_.reset(new cricket::ChannelManager(
      media_engine, device_manager, worker_thread));
  if (channel_manager_->Init()) {
    initialized_ = true;
  }
  return initialized_;
}

PeerConnectionManager::PeerConnectionManager()
    : signal_thread_(new talk_base::Thread) {

}

bool PeerConnectionManager::Initialize(cricket::PortAllocator* port_allocator,
                                       talk_base::Thread* worker_thread) {
  port_allocator_.reset(port_allocator);
  channel_manager_.reset(new cricket::ChannelManager(worker_thread));
  if (channel_manager_->Init()) {
    initialized_ = true;
  }
  return initialized_;
}

PeerConnection* PeerConnectionManager::CreatePeerConnection() {
  // TODO(mallinath) - It may be necessary to store the created PeerConnection
  // object in manager.
  return new PeerConnectionImpl(channel_manager_.get(),
                                port_allocator_.get(),
                                signal_thread_.get());
}

void PeerConnectionManager::DestroyPeerConnection(PeerConnection* pc) {
  delete static_cast<PeerConnectionImpl*> (pc);
}

} // namespace webrtc
