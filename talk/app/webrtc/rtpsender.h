/*
 * libjingle
 * Copyright 2015 Google Inc.
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

// This file contains classes that implement RtpSenderInterface.
// An RtpSender associates a MediaStreamTrackInterface with an underlying
// transport (provided by AudioProviderInterface/VideoProviderInterface)

#ifndef TALK_APP_WEBRTC_RTPSENDER_H_
#define TALK_APP_WEBRTC_RTPSENDER_H_

#include <string>

#include "talk/app/webrtc/mediastreamprovider.h"
#include "talk/app/webrtc/rtpsenderinterface.h"
#include "talk/media/base/audiorenderer.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ptr.h"

namespace webrtc {

// LocalAudioSinkAdapter receives data callback as a sink to the local
// AudioTrack, and passes the data to the sink of AudioRenderer.
class LocalAudioSinkAdapter : public AudioTrackSinkInterface,
                              public cricket::AudioRenderer {
 public:
  LocalAudioSinkAdapter();
  virtual ~LocalAudioSinkAdapter();

 private:
  // AudioSinkInterface implementation.
  void OnData(const void* audio_data,
              int bits_per_sample,
              int sample_rate,
              int number_of_channels,
              size_t number_of_frames) override;

  // cricket::AudioRenderer implementation.
  void SetSink(cricket::AudioRenderer::Sink* sink) override;

  cricket::AudioRenderer::Sink* sink_;
  // Critical section protecting |sink_|.
  rtc::CriticalSection lock_;
};

class AudioRtpSender : public ObserverInterface,
                       public rtc::RefCountedObject<RtpSenderInterface> {
 public:
  AudioRtpSender(AudioTrackInterface* track,
                 uint32_t ssrc,
                 AudioProviderInterface* provider);

  virtual ~AudioRtpSender();

  // ObserverInterface implementation
  void OnChanged() override;

  // RtpSenderInterface implementation
  bool SetTrack(MediaStreamTrackInterface* track) override;
  rtc::scoped_refptr<MediaStreamTrackInterface> track() const override {
    return track_.get();
  }

  std::string id() const override { return id_; }

  void Stop() override;

 private:
  void Reconfigure();

  std::string id_;
  rtc::scoped_refptr<AudioTrackInterface> track_;
  uint32_t ssrc_;
  AudioProviderInterface* provider_;
  bool cached_track_enabled_;

  // Used to pass the data callback from the |track_| to the other end of
  // cricket::AudioRenderer.
  rtc::scoped_ptr<LocalAudioSinkAdapter> sink_adapter_;
};

class VideoRtpSender : public ObserverInterface,
                       public rtc::RefCountedObject<RtpSenderInterface> {
 public:
  VideoRtpSender(VideoTrackInterface* track,
                 uint32_t ssrc,
                 VideoProviderInterface* provider);

  virtual ~VideoRtpSender();

  // ObserverInterface implementation
  void OnChanged() override;

  // RtpSenderInterface implementation
  bool SetTrack(MediaStreamTrackInterface* track) override;
  rtc::scoped_refptr<MediaStreamTrackInterface> track() const override {
    return track_.get();
  }

  std::string id() const override { return id_; }

  void Stop() override;

 private:
  void Reconfigure();

  std::string id_;
  rtc::scoped_refptr<VideoTrackInterface> track_;
  uint32_t ssrc_;
  VideoProviderInterface* provider_;
  bool cached_track_enabled_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_RTPSENDER_H_
