/*
 * libjingle
 * Copyright 2004--2011 Google Inc.
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
#include "talk/app/webrtc/mediastream.h"
#include "talk/app/webrtc/mediastreamproxy.h"
#include "talk/app/webrtc/mediastreamtrackproxy.h"
#include "talk/app/webrtc/peerconnection.h"
#include "talk/app/webrtc/peerconnectionfactoryproxy.h"
#include "talk/app/webrtc/peerconnectionproxy.h"
#include "talk/app/webrtc/portallocatorfactory.h"
#include "talk/app/webrtc/videosource.h"
#include "talk/app/webrtc/videosourceproxy.h"
#include "talk/app/webrtc/videotrack.h"
#include "talk/media/webrtc/webrtcmediaengine.h"
#include "talk/media/webrtc/webrtcvideodecoderfactory.h"
#include "talk/media/webrtc/webrtcvideoencoderfactory.h"
#include "webrtc/base/bind.h"
#include "webrtc/modules/audio_device/include/audio_device.h"

namespace webrtc {

namespace {

// Passes down the calls to |store_|. See usage in CreatePeerConnection.
class DtlsIdentityStoreWrapper : public DtlsIdentityStoreInterface {
 public:
  DtlsIdentityStoreWrapper(
      const rtc::scoped_refptr<RefCountedDtlsIdentityStore>& store)
      : store_(store) {
    RTC_DCHECK(store_);
  }

  void RequestIdentity(
      rtc::KeyType key_type,
      const rtc::scoped_refptr<webrtc::DtlsIdentityRequestObserver>&
          observer) override {
    store_->RequestIdentity(key_type, observer);
  }

 private:
  rtc::scoped_refptr<RefCountedDtlsIdentityStore> store_;
};

}  // anonymous namespace

rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactory() {
  rtc::scoped_refptr<PeerConnectionFactory> pc_factory(
      new rtc::RefCountedObject<PeerConnectionFactory>());


  // Call Initialize synchronously but make sure its executed on
  // |signaling_thread|.
  MethodCall0<PeerConnectionFactory, bool> call(
      pc_factory.get(),
      &PeerConnectionFactory::Initialize);
  bool result =  call.Marshal(pc_factory->signaling_thread());

  if (!result) {
    return NULL;
  }
  return PeerConnectionFactoryProxy::Create(pc_factory->signaling_thread(),
                                            pc_factory);
}

rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactory(
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    cricket::WebRtcVideoEncoderFactory* encoder_factory,
    cricket::WebRtcVideoDecoderFactory* decoder_factory) {
  rtc::scoped_refptr<PeerConnectionFactory> pc_factory(
      new rtc::RefCountedObject<PeerConnectionFactory>(worker_thread,
                                                       signaling_thread,
                                                       default_adm,
                                                       encoder_factory,
                                                       decoder_factory));

  // Call Initialize synchronously but make sure its executed on
  // |signaling_thread|.
  MethodCall0<PeerConnectionFactory, bool> call(
      pc_factory.get(),
      &PeerConnectionFactory::Initialize);
  bool result =  call.Marshal(signaling_thread);

  if (!result) {
    return NULL;
  }
  return PeerConnectionFactoryProxy::Create(signaling_thread, pc_factory);
}

PeerConnectionFactory::PeerConnectionFactory()
    : owns_ptrs_(true),
      wraps_current_thread_(false),
      signaling_thread_(rtc::ThreadManager::Instance()->CurrentThread()),
      worker_thread_(new rtc::Thread) {
  if (!signaling_thread_) {
    signaling_thread_ = rtc::ThreadManager::Instance()->WrapCurrentThread();
    wraps_current_thread_ = true;
  }
  worker_thread_->Start();
}

PeerConnectionFactory::PeerConnectionFactory(
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory)
    : owns_ptrs_(false),
      wraps_current_thread_(false),
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
  RTC_DCHECK(signaling_thread_->IsCurrent());
  channel_manager_.reset(nullptr);
  default_allocator_factory_ = nullptr;

  // Make sure |worker_thread_| and |signaling_thread_| outlive
  // |dtls_identity_store_|.
  dtls_identity_store_ = nullptr;

  if (owns_ptrs_) {
    if (wraps_current_thread_)
      rtc::ThreadManager::Instance()->UnwrapCurrentThread();
    delete worker_thread_;
  }
}

bool PeerConnectionFactory::Initialize() {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  rtc::InitRandom(rtc::Time());

  default_allocator_factory_ = PortAllocatorFactory::Create(worker_thread_);
  if (!default_allocator_factory_)
    return false;

  // TODO:  Need to make sure only one VoE is created inside
  // WebRtcMediaEngine.
  cricket::MediaEngineInterface* media_engine =
      worker_thread_->Invoke<cricket::MediaEngineInterface*>(rtc::Bind(
      &PeerConnectionFactory::CreateMediaEngine_w, this));

  channel_manager_.reset(
      new cricket::ChannelManager(media_engine, worker_thread_));

  channel_manager_->SetVideoRtxEnabled(true);
  if (!channel_manager_->Init()) {
    return false;
  }

  dtls_identity_store_ = new RefCountedDtlsIdentityStore(
      signaling_thread_, worker_thread_);

  return true;
}

rtc::scoped_refptr<AudioSourceInterface>
PeerConnectionFactory::CreateAudioSource(
    const MediaConstraintsInterface* constraints) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  rtc::scoped_refptr<LocalAudioSource> source(
      LocalAudioSource::Create(options_, constraints));
  return source;
}

rtc::scoped_refptr<VideoSourceInterface>
PeerConnectionFactory::CreateVideoSource(
    cricket::VideoCapturer* capturer,
    const MediaConstraintsInterface* constraints) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  rtc::scoped_refptr<VideoSource> source(
      VideoSource::Create(channel_manager_.get(), capturer, constraints));
  return VideoSourceProxy::Create(signaling_thread_, source);
}

bool PeerConnectionFactory::StartAecDump(rtc::PlatformFile file) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  return channel_manager_->StartAecDump(file);
}

void PeerConnectionFactory::StopAecDump() {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  channel_manager_->StopAecDump();
}

bool PeerConnectionFactory::StartRtcEventLog(rtc::PlatformFile file) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  return channel_manager_->StartRtcEventLog(file);
}

void PeerConnectionFactory::StopRtcEventLog() {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  channel_manager_->StopRtcEventLog();
}

rtc::scoped_refptr<PeerConnectionInterface>
PeerConnectionFactory::CreatePeerConnection(
    const PeerConnectionInterface::RTCConfiguration& configuration,
    const MediaConstraintsInterface* constraints,
    PortAllocatorFactoryInterface* allocator_factory,
    rtc::scoped_ptr<DtlsIdentityStoreInterface> dtls_identity_store,
    PeerConnectionObserver* observer) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  RTC_DCHECK(allocator_factory || default_allocator_factory_);

  if (!dtls_identity_store.get()) {
    // Because |pc|->Initialize takes ownership of the store we need a new
    // wrapper object that can be deleted without deleting the underlying
    // |dtls_identity_store_|, protecting it from being deleted multiple times.
    dtls_identity_store.reset(
        new DtlsIdentityStoreWrapper(dtls_identity_store_));
  }

  PortAllocatorFactoryInterface* chosen_allocator_factory =
      allocator_factory ? allocator_factory : default_allocator_factory_.get();
  chosen_allocator_factory->SetNetworkIgnoreMask(options_.network_ignore_mask);

  rtc::scoped_refptr<PeerConnection> pc(
      new rtc::RefCountedObject<PeerConnection>(this));
  if (!pc->Initialize(
      configuration,
      constraints,
      chosen_allocator_factory,
      dtls_identity_store.Pass(),
      observer)) {
    return NULL;
  }
  return PeerConnectionProxy::Create(signaling_thread(), pc);
}

rtc::scoped_refptr<MediaStreamInterface>
PeerConnectionFactory::CreateLocalMediaStream(const std::string& label) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  return MediaStreamProxy::Create(signaling_thread_,
                                  MediaStream::Create(label));
}

rtc::scoped_refptr<VideoTrackInterface>
PeerConnectionFactory::CreateVideoTrack(
    const std::string& id,
    VideoSourceInterface* source) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  rtc::scoped_refptr<VideoTrackInterface> track(
      VideoTrack::Create(id, source));
  return VideoTrackProxy::Create(signaling_thread_, track);
}

rtc::scoped_refptr<AudioTrackInterface>
PeerConnectionFactory::CreateAudioTrack(const std::string& id,
                                        AudioSourceInterface* source) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  rtc::scoped_refptr<AudioTrackInterface> track(
      AudioTrack::Create(id, source));
  return AudioTrackProxy::Create(signaling_thread_, track);
}

webrtc::MediaControllerInterface* PeerConnectionFactory::CreateMediaController()
    const {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  return MediaControllerInterface::Create(worker_thread_,
                                          channel_manager_.get());
}

rtc::Thread* PeerConnectionFactory::signaling_thread() {
  // This method can be called on a different thread when the factory is
  // created in CreatePeerConnectionFactory().
  return signaling_thread_;
}

rtc::Thread* PeerConnectionFactory::worker_thread() {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  return worker_thread_;
}

cricket::MediaEngineInterface* PeerConnectionFactory::CreateMediaEngine_w() {
  ASSERT(worker_thread_ == rtc::Thread::Current());
  return cricket::WebRtcMediaEngineFactory::Create(
      default_adm_.get(), video_encoder_factory_.get(),
      video_decoder_factory_.get());
}

}  // namespace webrtc
