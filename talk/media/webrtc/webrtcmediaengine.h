/*
 * libjingle
 * Copyright 2011 Google Inc.
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

#ifndef TALK_MEDIA_WEBRTCMEDIAENGINE_H_
#define TALK_MEDIA_WEBRTCMEDIAENGINE_H_

#include "talk/media/base/mediaengine.h"
#include "talk/media/webrtc/webrtcexport.h"

namespace webrtc {
class AudioDeviceModule;
class VideoCaptureModule;
}
namespace cricket {
class WebRtcVideoDecoderFactory;
class WebRtcVideoEncoderFactory;
}

#if !defined(LIBPEERCONNECTION_LIB) && \
    !defined(LIBPEERCONNECTION_IMPLEMENTATION)

WRME_EXPORT
cricket::MediaEngineInterface* CreateWebRtcMediaEngine(
    webrtc::AudioDeviceModule* adm, webrtc::AudioDeviceModule* adm_sc,
    cricket::WebRtcVideoEncoderFactory* encoder_factory,
    cricket::WebRtcVideoDecoderFactory* decoder_factory);

WRME_EXPORT
void DestroyWebRtcMediaEngine(cricket::MediaEngineInterface* media_engine);

#endif  // !defined(LIBPEERCONNECTION_LIB) &&
        // !defined(LIBPEERCONNECTION_IMPLEMENTATION)

namespace cricket {

class WebRtcMediaEngineFactory {
 public:
#if !defined(LIBPEERCONNECTION_LIB) && \
    !defined(LIBPEERCONNECTION_IMPLEMENTATION)
// A bare Create() isn't supported when using the delegating media
// engine.
#else
  static MediaEngineInterface* Create();
#endif  // !defined(LIBPEERCONNECTION_LIB) &&
        // !defined(LIBPEERCONNECTION_IMPLEMENTATION)
  static MediaEngineInterface* Create(
      webrtc::AudioDeviceModule* adm,
      webrtc::AudioDeviceModule* adm_sc,
      WebRtcVideoEncoderFactory* encoder_factory,
      WebRtcVideoDecoderFactory* decoder_factory);
};

}  // namespace cricket


#if !defined(LIBPEERCONNECTION_LIB) && \
    !defined(LIBPEERCONNECTION_IMPLEMENTATION)

namespace cricket {

// TODO(pthacther): Move this code into webrtcmediaengine.cc once
// Chrome compiles it.  Right now it relies on only the .h file.
class DelegatingWebRtcMediaEngine : public cricket::MediaEngineInterface {
 public:
  DelegatingWebRtcMediaEngine(
      webrtc::AudioDeviceModule* adm,
      webrtc::AudioDeviceModule* adm_sc,
      WebRtcVideoEncoderFactory* encoder_factory,
      WebRtcVideoDecoderFactory* decoder_factory)
      : delegate_(CreateWebRtcMediaEngine(
          adm, adm_sc, encoder_factory, decoder_factory)) {
  }
  virtual ~DelegatingWebRtcMediaEngine() {
    DestroyWebRtcMediaEngine(delegate_);
  }
  bool Init(rtc::Thread* worker_thread) override {
    return delegate_->Init(worker_thread);
  }
  void Terminate() override { delegate_->Terminate(); }
  int GetCapabilities() override { return delegate_->GetCapabilities(); }
  VoiceMediaChannel* CreateChannel() override {
    return delegate_->CreateChannel();
  }
  VideoMediaChannel* CreateVideoChannel(
      const VideoOptions& options,
      VoiceMediaChannel* voice_media_channel) override {
    return delegate_->CreateVideoChannel(options, voice_media_channel);
  }
  SoundclipMedia* CreateSoundclip() override {
    return delegate_->CreateSoundclip();
  }
  AudioOptions GetAudioOptions() const override {
    return delegate_->GetAudioOptions();
  }
  bool SetAudioOptions(const AudioOptions& options) override {
    return delegate_->SetAudioOptions(options);
  }
  bool SetAudioDelayOffset(int offset) override {
    return delegate_->SetAudioDelayOffset(offset);
  }
  bool SetDefaultVideoEncoderConfig(const VideoEncoderConfig& config) override {
    return delegate_->SetDefaultVideoEncoderConfig(config);
  }
  bool SetSoundDevices(const Device* in_device,
                       const Device* out_device) override {
    return delegate_->SetSoundDevices(in_device, out_device);
  }
  bool GetOutputVolume(int* level) override {
    return delegate_->GetOutputVolume(level);
  }
  bool SetOutputVolume(int level) override {
    return delegate_->SetOutputVolume(level);
  }
  int GetInputLevel() override { return delegate_->GetInputLevel(); }
  bool SetLocalMonitor(bool enable) override {
    return delegate_->SetLocalMonitor(enable);
  }
  const std::vector<AudioCodec>& audio_codecs() override {
    return delegate_->audio_codecs();
  }
  const std::vector<RtpHeaderExtension>& audio_rtp_header_extensions()
      override {
    return delegate_->audio_rtp_header_extensions();
  }
  const std::vector<VideoCodec>& video_codecs() override {
    return delegate_->video_codecs();
  }
  const std::vector<RtpHeaderExtension>& video_rtp_header_extensions()
      override {
    return delegate_->video_rtp_header_extensions();
  }
  void SetVoiceLogging(int min_sev, const char* filter) override {
    delegate_->SetVoiceLogging(min_sev, filter);
  }
  void SetVideoLogging(int min_sev, const char* filter) override {
    delegate_->SetVideoLogging(min_sev, filter);
  }
  bool StartAecDump(rtc::PlatformFile file) override {
    return delegate_->StartAecDump(file);
  }
  bool RegisterVoiceProcessor(uint32 ssrc,
                              VoiceProcessor* video_processor,
                              MediaProcessorDirection direction) override {
    return delegate_->RegisterVoiceProcessor(ssrc, video_processor, direction);
  }
  bool UnregisterVoiceProcessor(uint32 ssrc,
                                VoiceProcessor* video_processor,
                                MediaProcessorDirection direction) override {
    return delegate_->UnregisterVoiceProcessor(ssrc, video_processor,
        direction);
  }
  VideoFormat GetStartCaptureFormat() const override {
    return delegate_->GetStartCaptureFormat();
  }
  virtual sigslot::repeater2<VideoCapturer*, CaptureState>&
      SignalVideoCaptureStateChange() {
    return delegate_->SignalVideoCaptureStateChange();
  }

 private:
  cricket::MediaEngineInterface* delegate_;
};

// Used by PeerConnectionFactory to create a media engine passed into
// ChannelManager.
MediaEngineInterface* WebRtcMediaEngineFactory::Create(
    webrtc::AudioDeviceModule* adm,
    webrtc::AudioDeviceModule* adm_sc,
    WebRtcVideoEncoderFactory* encoder_factory,
    WebRtcVideoDecoderFactory* decoder_factory) {
  return new cricket::DelegatingWebRtcMediaEngine(
      adm, adm_sc, encoder_factory, decoder_factory);
}

}  // namespace cricket

#endif  // !defined(LIBPEERCONNECTION_LIB) &&
        // !defined(LIBPEERCONNECTION_IMPLEMENTATION)

#endif  // TALK_MEDIA_WEBRTCMEDIAENGINE_H_
