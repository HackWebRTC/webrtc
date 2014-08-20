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
  virtual bool Init(rtc::Thread* worker_thread) OVERRIDE {
    return delegate_->Init(worker_thread);
  }
  virtual void Terminate() OVERRIDE {
    delegate_->Terminate();
  }
  virtual int GetCapabilities() OVERRIDE {
    return delegate_->GetCapabilities();
  }
  virtual VoiceMediaChannel* CreateChannel() OVERRIDE {
    return delegate_->CreateChannel();
  }
  virtual VideoMediaChannel* CreateVideoChannel(
      VoiceMediaChannel* voice_media_channel) OVERRIDE {
    return delegate_->CreateVideoChannel(voice_media_channel);
  }
  virtual SoundclipMedia* CreateSoundclip() OVERRIDE {
    return delegate_->CreateSoundclip();
  }
  virtual AudioOptions GetAudioOptions() const OVERRIDE {
    return delegate_->GetAudioOptions();
  }
  virtual bool SetAudioOptions(const AudioOptions& options) OVERRIDE {
    return delegate_->SetAudioOptions(options);
  }
  virtual bool SetVideoOptions(const VideoOptions& options) OVERRIDE {
    return delegate_->SetVideoOptions(options);
  }
  virtual bool SetAudioDelayOffset(int offset) OVERRIDE {
    return delegate_->SetAudioDelayOffset(offset);
  }
  virtual bool SetDefaultVideoEncoderConfig(
      const VideoEncoderConfig& config) OVERRIDE {
    return delegate_->SetDefaultVideoEncoderConfig(config);
  }
  virtual VideoEncoderConfig GetDefaultVideoEncoderConfig() const OVERRIDE {
    return delegate_->GetDefaultVideoEncoderConfig();
  }
  virtual bool SetSoundDevices(
      const Device* in_device, const Device* out_device) OVERRIDE {
    return delegate_->SetSoundDevices(in_device, out_device);
  }
  virtual bool GetOutputVolume(int* level) OVERRIDE {
    return delegate_->GetOutputVolume(level);
  }
  virtual bool SetOutputVolume(int level) OVERRIDE {
    return delegate_->SetOutputVolume(level);
  }
  virtual int GetInputLevel() OVERRIDE {
    return delegate_->GetInputLevel();
  }
  virtual bool SetLocalMonitor(bool enable) OVERRIDE {
    return delegate_->SetLocalMonitor(enable);
  }
  virtual bool SetLocalRenderer(VideoRenderer* renderer) OVERRIDE {
    return delegate_->SetLocalRenderer(renderer);
  }
  virtual const std::vector<AudioCodec>& audio_codecs() OVERRIDE {
    return delegate_->audio_codecs();
  }
  virtual const std::vector<RtpHeaderExtension>&
      audio_rtp_header_extensions() OVERRIDE {
    return delegate_->audio_rtp_header_extensions();
  }
  virtual const std::vector<VideoCodec>& video_codecs() OVERRIDE {
    return delegate_->video_codecs();
  }
  virtual const std::vector<RtpHeaderExtension>&
      video_rtp_header_extensions() OVERRIDE {
    return delegate_->video_rtp_header_extensions();
  }
  virtual void SetVoiceLogging(int min_sev, const char* filter) OVERRIDE {
    delegate_->SetVoiceLogging(min_sev, filter);
  }
  virtual void SetVideoLogging(int min_sev, const char* filter) OVERRIDE {
    delegate_->SetVideoLogging(min_sev, filter);
  }
  virtual bool StartAecDump(rtc::PlatformFile file) OVERRIDE {
    return delegate_->StartAecDump(file);
  }
  virtual bool RegisterVoiceProcessor(
      uint32 ssrc, VoiceProcessor* video_processor,
      MediaProcessorDirection direction) OVERRIDE {
    return delegate_->RegisterVoiceProcessor(ssrc, video_processor, direction);
  }
  virtual bool UnregisterVoiceProcessor(
      uint32 ssrc, VoiceProcessor* video_processor,
      MediaProcessorDirection direction) OVERRIDE {
    return delegate_->UnregisterVoiceProcessor(ssrc, video_processor,
        direction);
  }
  virtual VideoFormat GetStartCaptureFormat() const OVERRIDE {
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
