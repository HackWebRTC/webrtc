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

#include "talk/app/webrtc_dev/peerconnectionfactoryimpl.h"

#include "talk/app/webrtc_dev/mediastreamproxy.h"
#include "talk/app/webrtc_dev/mediastreamtrackproxy.h"
#include "talk/app/webrtc_dev/peerconnectionimpl.h"
#include "talk/app/webrtc_dev/webrtc_devicemanager.h"
#include "talk/base/basicpacketsocketfactory.h"
#include "talk/session/phone/webrtcmediaengine.h"

#ifdef WEBRTC_RELATIVE_PATH
#include "modules/audio_device/main/interface/audio_device.h"
#else
#include "third_party/webrtc/files/include/audio_device.h"
#endif

using talk_base::scoped_refptr;

namespace {

typedef talk_base::TypedMessageData<bool> InitMessageData;

struct CreatePeerConnectionParams : public talk_base::MessageData {
  CreatePeerConnectionParams(const std::string& configuration,
                             webrtc::PeerConnectionObserver* observer)
      : configuration(configuration), observer(observer) {
  }
  scoped_refptr<webrtc::PeerConnectionInterface> peerconnection;
  const std::string& configuration;
  webrtc::PeerConnectionObserver* observer;
};

enum {
  MSG_INIT_FACTORY = 1,
  MSG_CREATE_PEERCONNECTION = 2,
};

}  // namespace anonymous


namespace webrtc {
scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactory() {
  talk_base::RefCountedObject<PeerConnectionFactoryImpl>* pc_factory =
      new talk_base::RefCountedObject<PeerConnectionFactoryImpl>();

  if (!pc_factory->Initialize()) {
    delete pc_factory;
    pc_factory = NULL;
  }
  return pc_factory;
}

scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactory(
    talk_base::Thread* worker_thread,
    talk_base::Thread* signaling_thread,
    talk_base::NetworkManager* network_manager,
    talk_base::PacketSocketFactory* socket_factory,
    AudioDeviceModule* default_adm) {
  talk_base::RefCountedObject<PeerConnectionFactoryImpl>* pc_factory =
      new talk_base::RefCountedObject<PeerConnectionFactoryImpl>(
          worker_thread, signaling_thread, network_manager, socket_factory,
          default_adm);
  if (!pc_factory->Initialize()) {
    delete pc_factory;
    pc_factory = NULL;
  }
  return pc_factory;
}

PeerConnectionFactoryImpl::PeerConnectionFactoryImpl()
    : worker_thread_(new talk_base::Thread),
      signaling_thread_(new talk_base::Thread) {
  worker_thread_ptr_ = worker_thread_.get();
  signaling_thread_ptr_ = signaling_thread_.get();
  bool result = worker_thread_->Start();
  ASSERT(result);
  result = signaling_thread_->Start();
  ASSERT(result);
}

PeerConnectionFactoryImpl::PeerConnectionFactoryImpl(
    talk_base::Thread* worker_thread,
    talk_base::Thread* signaling_thread,
    talk_base::NetworkManager* network_manager,
    talk_base::PacketSocketFactory* socket_factory,
    AudioDeviceModule* default_adm)
    : worker_thread_ptr_(worker_thread),
      signaling_thread_ptr_(signaling_thread),
      network_manager_(network_manager),
      socket_factory_(socket_factory),
      default_adm_(default_adm) {
  ASSERT(worker_thread);
  ASSERT(signaling_thread);
  ASSERT(network_manager);
  ASSERT(socket_factory);
  ASSERT(default_adm);
}

PeerConnectionFactoryImpl::~PeerConnectionFactoryImpl() {
}

bool PeerConnectionFactoryImpl::Initialize() {
  InitMessageData result(false);
  signaling_thread_ptr_->Send(this, MSG_INIT_FACTORY, &result);
  return result.data();
}

void PeerConnectionFactoryImpl::OnMessage(talk_base::Message* msg) {
  switch (msg->message_id) {
    case MSG_INIT_FACTORY: {
     InitMessageData* pdata = static_cast<InitMessageData*> (msg->pdata);
     pdata->data() = Initialize_s();
     break;
    }
    case MSG_CREATE_PEERCONNECTION: {
      CreatePeerConnectionParams* pdata =
          static_cast<CreatePeerConnectionParams*> (msg->pdata);
      pdata->peerconnection = CreatePeerConnection_s(pdata->configuration,
                                                     pdata->observer);
      break;
    }
  }
}

bool PeerConnectionFactoryImpl::Initialize_s() {
  if (!network_manager_.get())
    network_manager_.reset(new talk_base::BasicNetworkManager());
  if (!socket_factory_.get())
    socket_factory_.reset(
        new talk_base::BasicPacketSocketFactory(worker_thread_ptr_));

  cricket::DeviceManager* device_manager(new WebRtcDeviceManager());
  // TODO(perkj):  Need to make sure only one VoE is created inside
  // WebRtcMediaEngine.
  cricket::WebRtcMediaEngine* webrtc_media_engine(
      new cricket::WebRtcMediaEngine(default_adm_.get(),
                                     NULL,   // No secondary adm.
                                     NULL));  // No vcm available.

  channel_manager_.reset(new cricket::ChannelManager(
      webrtc_media_engine, device_manager, worker_thread_ptr_));
  if (!channel_manager_->Init()) {
    return false;
  }
  return true;
}

scoped_refptr<PeerConnectionInterface>
PeerConnectionFactoryImpl::CreatePeerConnection(
    const std::string& configuration,
    PeerConnectionObserver* observer) {
  CreatePeerConnectionParams params(configuration, observer);
  signaling_thread_ptr_->Send(this, MSG_CREATE_PEERCONNECTION, &params);
  return params.peerconnection;
}

scoped_refptr<PeerConnectionInterface>
PeerConnectionFactoryImpl::CreatePeerConnection_s(
    const std::string& configuration,
    PeerConnectionObserver* observer) {
  talk_base::RefCountedObject<PeerConnectionImpl>* pc(
      new talk_base::RefCountedObject<PeerConnectionImpl>(this));
  if (!pc->Initialize(configuration, observer)) {
    delete pc;
    pc = NULL;
  }
  return pc;
}

scoped_refptr<LocalMediaStreamInterface>
PeerConnectionFactoryImpl::CreateLocalMediaStream(
      const std::string& label) {
  return MediaStreamProxy::Create(label, signaling_thread_ptr_);
}

scoped_refptr<LocalVideoTrackInterface>
PeerConnectionFactoryImpl::CreateLocalVideoTrack(
    const std::string& label,
    VideoCaptureModule* video_device) {
  return VideoTrackProxy::CreateLocal(label, video_device,
                                      signaling_thread_ptr_);
}

scoped_refptr<LocalAudioTrackInterface>
PeerConnectionFactoryImpl::CreateLocalAudioTrack(
    const std::string& label,
    AudioDeviceModule* audio_device) {
  return AudioTrackProxy::CreateLocal(label, audio_device,
                                      signaling_thread_ptr_);
}

cricket::ChannelManager* PeerConnectionFactoryImpl::channel_manager() {
  return channel_manager_.get();
}

talk_base::Thread* PeerConnectionFactoryImpl::signaling_thread() {
  return signaling_thread_ptr_;
}

talk_base::Thread* PeerConnectionFactoryImpl::worker_thread() {
  return worker_thread_ptr_;
}

talk_base::NetworkManager* PeerConnectionFactoryImpl::network_manager() {
  return network_manager_.get();
}

talk_base::PacketSocketFactory* PeerConnectionFactoryImpl::socket_factory() {
  return socket_factory_.get();
}

}  // namespace webrtc
