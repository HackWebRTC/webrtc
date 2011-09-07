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

#ifndef TALK_APP_WEBRTC_MEDIA_ENGINE_H_
#define TALK_APP_WEBRTC_MEDIA_ENGINE_H_

#include <vector>

#include "talk/session/phone/mediaengine.h"

namespace cricket {
class WebRtcVideoEngine;
class WebRtcVoiceEngine;
}

namespace webrtc {
class AudioDeviceModule;
class VideoCaptureModule;

// TODO(perkj) Write comments. Why do we need to override cricket::MediaEngine.
class WebRtcMediaEngine : public cricket::MediaEngine {
 public:
  WebRtcMediaEngine();
  WebRtcMediaEngine(webrtc::AudioDeviceModule* adm,
      webrtc::AudioDeviceModule* adm_sc, webrtc::VideoCaptureModule* vcm);
  virtual ~WebRtcMediaEngine();
  virtual bool Init();
  virtual void Terminate();
  virtual int GetCapabilities();
  virtual cricket::VoiceMediaChannel *CreateChannel();
  virtual cricket::VideoMediaChannel *CreateVideoChannel(
      cricket::VoiceMediaChannel* channel);
  virtual cricket::SoundclipMedia *CreateSoundclip();
  virtual bool SetAudioOptions(int o);
  virtual bool SetVideoOptions(int o);
  virtual bool SetDefaultVideoEncoderConfig(
      const cricket::VideoEncoderConfig& config);
  virtual bool SetSoundDevices(const cricket::Device* in_device,
                               const cricket::Device* out_device);
  virtual bool SetVideoCaptureDevice(const cricket::Device* cam_device);
  virtual bool GetOutputVolume(int* level);
  virtual bool SetOutputVolume(int level);
  virtual int GetInputLevel();
  virtual bool SetLocalMonitor(bool enable);
  virtual bool SetLocalRenderer(cricket::VideoRenderer* renderer);
  virtual cricket::CaptureResult SetVideoCapture(bool capture);
  virtual const std::vector<cricket::AudioCodec>& audio_codecs();
  virtual const std::vector<cricket::VideoCodec>& video_codecs();
  virtual void SetVoiceLogging(int min_sev, const char* filter);
  virtual void SetVideoLogging(int min_sev, const char* filter);

  // Allow the VCM be set later if not ready during the construction time
  bool SetVideoCaptureModule(webrtc::VideoCaptureModule* vcm);

 protected:
  cricket::WebRtcVoiceEngine* voice_;
  cricket::WebRtcVideoEngine* video_;
};

}  // namespace WebRtcMediaEngine

#endif  // TALK_APP_WEBRTC_MEDIA_ENGINE_H_
