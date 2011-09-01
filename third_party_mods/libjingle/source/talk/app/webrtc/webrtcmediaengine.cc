/*
 * libjingle
 * Copyright 2011, Google Inc.
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

#include "talk/app/webrtc/webrtcmediaengine.h"

#include <vector>

#include "talk/session/phone/webrtcvoiceengine.h"
#include "talk/session/phone/webrtcvideoengine.h"

WebRtcMediaEngine::WebRtcMediaEngine(webrtc::AudioDeviceModule* adm,
    webrtc::AudioDeviceModule* adm_sc, webrtc::VideoCaptureModule* vcm)
    : voice_(new cricket::WebRtcVoiceEngine(adm, adm_sc)),
      video_(new cricket::WebRtcVideoEngine(voice_.get(), vcm)) {
}

WebRtcMediaEngine::~WebRtcMediaEngine() {
}

bool WebRtcMediaEngine::Init() {
  if (!voice_->Init())
    return false;
  if (!video_->Init()) {
    voice_->Terminate();
    return false;
  }
  SignalVideoCaptureResult.repeat(video_->SignalCaptureResult);
  return true;
}

void WebRtcMediaEngine::Terminate() {
  video_->Terminate();
  voice_->Terminate();
}

int WebRtcMediaEngine::GetCapabilities() {
  return (voice_->GetCapabilities() | video_->GetCapabilities());
}

cricket::VoiceMediaChannel* WebRtcMediaEngine::CreateChannel() {
  return voice_->CreateChannel();
}

cricket::VideoMediaChannel* WebRtcMediaEngine::CreateVideoChannel(
    cricket::VoiceMediaChannel* channel) {
  return video_->CreateChannel(channel);
}

cricket::SoundclipMedia* WebRtcMediaEngine::CreateSoundclip() {
  return voice_->CreateSoundclip();
}

bool WebRtcMediaEngine::SetAudioOptions(int o) {
  return voice_->SetOptions(o);
}

bool WebRtcMediaEngine::SetVideoOptions(int o) {
  return video_->SetOptions(o);
}

bool WebRtcMediaEngine::SetDefaultVideoEncoderConfig(
    const cricket::VideoEncoderConfig& config) {
  return video_->SetDefaultEncoderConfig(config);
}

bool WebRtcMediaEngine::SetSoundDevices(const cricket::Device* in_device,
                             const cricket::Device* out_device) {
  return voice_->SetDevices(in_device, out_device);
}

bool WebRtcMediaEngine::SetVideoCaptureDevice(
    const cricket::Device* cam_device) {
  return video_->SetCaptureDevice(cam_device);
}

bool WebRtcMediaEngine::GetOutputVolume(int* level) {
  return voice_->GetOutputVolume(level);
}

bool WebRtcMediaEngine::SetOutputVolume(int level) {
  return voice_->SetOutputVolume(level);
}

int WebRtcMediaEngine::GetInputLevel() {
  return voice_->GetInputLevel();
}

bool WebRtcMediaEngine::SetLocalMonitor(bool enable) {
  return voice_->SetLocalMonitor(enable);
}

bool WebRtcMediaEngine::SetLocalRenderer(cricket::VideoRenderer* renderer) {
  return video_->SetLocalRenderer(renderer);
}

cricket::CaptureResult WebRtcMediaEngine::SetVideoCapture(bool capture) {
  return video_->SetCapture(capture);
}

const std::vector<cricket::AudioCodec>& WebRtcMediaEngine::audio_codecs() {
  return voice_->codecs();
}

const std::vector<cricket::VideoCodec>& WebRtcMediaEngine::video_codecs() {
  return video_->codecs();
}

void WebRtcMediaEngine::SetVoiceLogging(int min_sev, const char* filter) {
  return voice_->SetLogging(min_sev, filter);
}

void WebRtcMediaEngine::SetVideoLogging(int min_sev, const char* filter) {
  return video_->SetLogging(min_sev, filter);
}

bool WebRtcMediaEngine::SetVideoCaptureModule(
    webrtc::VideoCaptureModule* vcm) {
  return video_->SetCaptureModule(vcm);
}
