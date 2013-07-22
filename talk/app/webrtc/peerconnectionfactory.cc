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

#include "talk/app/webrtc/peerconnectionfactory.h"

#include "talk/app/webrtc/audiotrack.h"
#include "talk/app/webrtc/localaudiosource.h"
#include "talk/app/webrtc/localvideosource.h"
#include "talk/app/webrtc/mediastreamproxy.h"
#include "talk/app/webrtc/mediastreamtrackproxy.h"
#include "talk/app/webrtc/peerconnection.h"
#include "talk/app/webrtc/peerconnectionproxy.h"
#include "talk/app/webrtc/portallocatorfactory.h"
#include "talk/app/webrtc/videosourceproxy.h"
#include "talk/app/webrtc/videotrack.h"
#include "talk/media/devices/dummydevicemanager.h"
#include "talk/media/webrtc/webrtcmediaengine.h"
#include "talk/media/webrtc/webrtcvideodecoderfactory.h"
#include "talk/media/webrtc/webrtcvideoencoderfactory.h"
#include "webrtc/modules/audio_device/include/audio_device.h"

using talk_base::scoped_refptr;

namespace {

typedef talk_base::TypedMessageData<bool> InitMessageData;

struct CreatePeerConnectionParams : public talk_base::MessageData {
  CreatePeerConnectionParams(
      const webrtc::PeerConnectionInterface::IceServers& configuration,
      const webrtc::MediaConstraintsInterface* constraints,
      webrtc::PortAllocatorFactoryInterface* allocator_factory,
      webrtc::PeerConnectionObserver* observer)
      : configuration(configuration),
        constraints(constraints),
        allocator_factory(allocator_factory),
        observer(observer) {
  }
  scoped_refptr<webrtc::PeerConnectionInterface> peerconnection;
  const webrtc::PeerConnectionInterface::IceServers& configuration;
  const webrtc::MediaConstraintsInterface* constraints;
  scoped_refptr<webrtc::PortAllocatorFactoryInterface> allocator_factory;
  webrtc::PeerConnectionObserver* observer;
};

struct CreatePeerConnectionParamsDeprecated : public talk_base::MessageData {
  CreatePeerConnectionParamsDeprecated(
      const std::string& configuration,
      webrtc::PortAllocatorFactoryInterface* allocator_factory,
        webrtc::PeerConnectionObserver* observer)
      : configuration(configuration),
        allocator_factory(allocator_factory),
        observer(observer) {
  }
  scoped_refptr<webrtc::PeerConnectionInterface> peerconnection;
  const std::string& configuration;
  scoped_refptr<webrtc::PortAllocatorFactoryInterface> allocator_factory;
  webrtc::PeerConnectionObserver* observer;
};

struct CreateAudioSourceParams : public talk_base::MessageData {
  explicit CreateAudioSourceParams(
      const webrtc::MediaConstraintsInterface* constraints)
      : constraints(constraints) {
  }
  const webrtc::MediaConstraintsInterface* constraints;
  scoped_refptr<webrtc::AudioSourceInterface> source;
};

struct CreateVideoSourceParams : public talk_base::MessageData {
  CreateVideoSourceParams(cricket::VideoCapturer* capturer,
                          const webrtc::MediaConstraintsInterface* constraints)
      : capturer(capturer),
        constraints(constraints) {
  }
  cricket::VideoCapturer* capturer;
  const webrtc::MediaConstraintsInterface* constraints;
  scoped_refptr<webrtc::VideoSourceInterface> source;
};

enum {
  MSG_INIT_FACTORY = 1,
  MSG_TERMINATE_FACTORY,
  MSG_CREATE_PEERCONNECTION,
  MSG_CREATE_AUDIOSOURCE,
  MSG_CREATE_VIDEOSOURCE,
};

}  // namespace

namespace webrtc {

scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactory() {
  scoped_refptr<PeerConnectionFactory> pc_factory(
      new talk_base::RefCountedObject<PeerConnectionFactory>());

  if (!pc_factory->Initialize()) {
    return NULL;
  }
  return pc_factory;
}

scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactory(
    talk_base::Thread* worker_thread,
    talk_base::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    cricket::WebRtcVideoEncoderFactory* encoder_factory,
    cricket::WebRtcVideoDecoderFactory* decoder_factory) {
  scoped_refptr<PeerConnectionFactory> pc_factory(
      new talk_base::RefCountedObject<PeerConnectionFactory>(
          worker_thread, signaling_thread, default_adm,
          encoder_factory, decoder_factory));
  if (!pc_factory->Initialize()) {
    return NULL;
  }
  return pc_factory;
}

PeerConnectionFactory::PeerConnectionFactory()
    : owns_ptrs_(true),
      signaling_thread_(new talk_base::Thread),
      worker_thread_(new talk_base::Thread) {
  bool result = signaling_thread_->Start();
  ASSERT(result);
  result = worker_thread_->Start();
  ASSERT(result);
}

PeerConnectionFactory::PeerConnectionFactory(
    talk_base::Thread* worker_thread,
    talk_base::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory)
    : owns_ptrs_(false),
      signaling_thread_(signaling_thread),
      worker_thread_(worker_thread),
      default_adm_(default_adm),
      video_encoder_factory_(video_encoder_factory),
      video_decoder_factory_(video_decoder_factory) {
  ASSERT(worker_thread != NULL);
  ASSERT(signaling_thread != NULL);
  // TODO: Currently there is no way creating an external adm in
  // libjingle source tree. So we can 't currently assert if this is NULL.
  // ASSERT(default_adm != NULL);
}

PeerConnectionFactory::~PeerConnectionFactory() {
  signaling_thread_->Clear(this);
  signaling_thread_->Send(this, MSG_TERMINATE_FACTORY);
  if (owns_ptrs_) {
    delete signaling_thread_;
    delete worker_thread_;
  }
}

bool PeerConnectionFactory::Initialize() {
  InitMessageData result(false);
  signaling_thread_->Send(this, MSG_INIT_FACTORY, &result);
  return result.data();
}

void PeerConnectionFactory::OnMessage(talk_base::Message* msg) {
  switch (msg->message_id) {
    case MSG_INIT_FACTORY: {
     InitMessageData* pdata = static_cast<InitMessageData*>(msg->pdata);
     pdata->data() = Initialize_s();
     break;
    }
    case MSG_TERMINATE_FACTORY: {
      Terminate_s();
      break;
    }
    case MSG_CREATE_PEERCONNECTION: {
      CreatePeerConnectionParams* pdata =
          static_cast<CreatePeerConnectionParams*>(msg->pdata);
      pdata->peerconnection = CreatePeerConnection_s(pdata->configuration,
                                                     pdata->constraints,
                                                     pdata->allocator_factory,
                                                     pdata->observer);
      break;
    }
    case MSG_CREATE_AUDIOSOURCE: {
      CreateAudioSourceParams* pdata =
          static_cast<CreateAudioSourceParams*>(msg->pdata);
      pdata->source = CreateAudioSource_s(pdata->constraints);
      break;
    }
    case MSG_CREATE_VIDEOSOURCE: {
      CreateVideoSourceParams* pdata =
          static_cast<CreateVideoSourceParams*>(msg->pdata);
      pdata->source = CreateVideoSource_s(pdata->capturer, pdata->constraints);
      break;
    }
  }
}

bool PeerConnectionFactory::Initialize_s() {
  talk_base::InitRandom(talk_base::Time());

  allocator_factory_ = PortAllocatorFactory::Create(worker_thread_);
  if (!allocator_factory_)
    return false;

  cricket::DummyDeviceManager* device_manager(
      new cricket::DummyDeviceManager());
  // TODO:  Need to make sure only one VoE is created inside
  // WebRtcMediaEngine.
  cricket::WebRtcMediaEngine* webrtc_media_engine(
      new cricket::WebRtcMediaEngine(default_adm_.get(),
                                     NULL,  // No secondary adm.
                                     video_encoder_factory_.get(),
                                     video_decoder_factory_.get()));

  channel_manager_.reset(new cricket::ChannelManager(
      webrtc_media_engine, device_manager, worker_thread_));
  if (!channel_manager_->Init()) {
    return false;
  }
  return true;
}

// Terminate what we created on the signaling thread.
void PeerConnectionFactory::Terminate_s() {
  channel_manager_.reset(NULL);
  allocator_factory_ = NULL;
}

talk_base::scoped_refptr<AudioSourceInterface>
PeerConnectionFactory::CreateAudioSource_s(
    const MediaConstraintsInterface* constraints) {
  talk_base::scoped_refptr<LocalAudioSource> source(
      LocalAudioSource::Create(constraints));
  return source;
}

talk_base::scoped_refptr<VideoSourceInterface>
PeerConnectionFactory::CreateVideoSource_s(
    cricket::VideoCapturer* capturer,
    const MediaConstraintsInterface* constraints) {
  talk_base::scoped_refptr<LocalVideoSource> source(
      LocalVideoSource::Create(channel_manager_.get(), capturer,
                               constraints));
  return VideoSourceProxy::Create(signaling_thread_, source);
}

scoped_refptr<PeerConnectionInterface>
PeerConnectionFactory::CreatePeerConnection(
    const PeerConnectionInterface::IceServers& configuration,
    const MediaConstraintsInterface* constraints,
    PortAllocatorFactoryInterface* allocator_factory,
    DTLSIdentityServiceInterface* dtls_identity_service,
    PeerConnectionObserver* observer) {
  CreatePeerConnectionParams params(configuration, constraints,
                                    allocator_factory, observer);
  signaling_thread_->Send(this, MSG_CREATE_PEERCONNECTION, &params);
  return params.peerconnection;
}

scoped_refptr<PeerConnectionInterface>
PeerConnectionFactory::CreatePeerConnection(
    const PeerConnectionInterface::IceServers& configuration,
    const MediaConstraintsInterface* constraints,
    DTLSIdentityServiceInterface* dtls_identity_service,
    PeerConnectionObserver* observer) {
  return CreatePeerConnection(
      configuration, constraints, NULL, dtls_identity_service, observer);
}

talk_base::scoped_refptr<PeerConnectionInterface>
PeerConnectionFactory::CreatePeerConnection_s(
    const PeerConnectionInterface::IceServers& configuration,
    const MediaConstraintsInterface* constraints,
    PortAllocatorFactoryInterface* allocator_factory,
    PeerConnectionObserver* observer) {
  ASSERT(allocator_factory || allocator_factory_);
  talk_base::scoped_refptr<PeerConnection> pc(
      new talk_base::RefCountedObject<PeerConnection>(this));
  if (!pc->Initialize(
      configuration,
      constraints,
      allocator_factory ? allocator_factory : allocator_factory_.get(),
      observer)) {
    return NULL;
  }
  return PeerConnectionProxy::Create(signaling_thread(), pc);
}

scoped_refptr<MediaStreamInterface>
PeerConnectionFactory::CreateLocalMediaStream(const std::string& label) {
  return MediaStreamProxy::Create(signaling_thread_,
                                  MediaStream::Create(label));
}

talk_base::scoped_refptr<AudioSourceInterface>
PeerConnectionFactory::CreateAudioSource(
    const MediaConstraintsInterface* constraints) {
  CreateAudioSourceParams params(constraints);
  signaling_thread_->Send(this, MSG_CREATE_AUDIOSOURCE, &params);
  return params.source;
}

talk_base::scoped_refptr<VideoSourceInterface>
PeerConnectionFactory::CreateVideoSource(
    cricket::VideoCapturer* capturer,
    const MediaConstraintsInterface* constraints) {

  CreateVideoSourceParams params(capturer,
                                 constraints);
  signaling_thread_->Send(this, MSG_CREATE_VIDEOSOURCE, &params);
  return params.source;
}

talk_base::scoped_refptr<VideoTrackInterface>
PeerConnectionFactory::CreateVideoTrack(
    const std::string& id,
    VideoSourceInterface* source) {
  talk_base::scoped_refptr<VideoTrackInterface> track(
      VideoTrack::Create(id, source));
  return VideoTrackProxy::Create(signaling_thread_, track);
}

scoped_refptr<AudioTrackInterface> PeerConnectionFactory::CreateAudioTrack(
    const std::string& id,
    AudioSourceInterface* source) {
  talk_base::scoped_refptr<AudioTrackInterface> track(
      AudioTrack::Create(id, source));
  return AudioTrackProxy::Create(signaling_thread_, track);
}

cricket::ChannelManager* PeerConnectionFactory::channel_manager() {
  return channel_manager_.get();
}

talk_base::Thread* PeerConnectionFactory::signaling_thread() {
  return signaling_thread_;
}

talk_base::Thread* PeerConnectionFactory::worker_thread() {
  return worker_thread_;
}

}  // namespace webrtc
