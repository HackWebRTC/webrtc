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

// This file contains classes that implement RtpReceiverInterface.
// An RtpReceiver associates a MediaStreamTrackInterface with an underlying
// transport (provided by AudioProviderInterface/VideoProviderInterface)

#ifndef TALK_APP_WEBRTC_RTPRECEIVER_H_
#define TALK_APP_WEBRTC_RTPRECEIVER_H_

#include <string>

#include "talk/app/webrtc/mediastreamprovider.h"
#include "talk/app/webrtc/rtpreceiverinterface.h"
#include "webrtc/base/basictypes.h"

namespace webrtc {

class AudioRtpReceiver : public ObserverInterface,
                         public AudioSourceInterface::AudioObserver,
                         public rtc::RefCountedObject<RtpReceiverInterface> {
 public:
  AudioRtpReceiver(AudioTrackInterface* track,
                   uint32_t ssrc,
                   AudioProviderInterface* provider);

  virtual ~AudioRtpReceiver();

  // ObserverInterface implementation
  void OnChanged() override;

  // AudioSourceInterface::AudioObserver implementation
  void OnSetVolume(double volume) override;

  // RtpReceiverInterface implementation
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
};

class VideoRtpReceiver : public rtc::RefCountedObject<RtpReceiverInterface> {
 public:
  VideoRtpReceiver(VideoTrackInterface* track,
                   uint32_t ssrc,
                   VideoProviderInterface* provider);

  virtual ~VideoRtpReceiver();

  // RtpReceiverInterface implementation
  rtc::scoped_refptr<MediaStreamTrackInterface> track() const override {
    return track_.get();
  }

  std::string id() const override { return id_; }

  void Stop() override;

 private:
  std::string id_;
  rtc::scoped_refptr<VideoTrackInterface> track_;
  uint32_t ssrc_;
  VideoProviderInterface* provider_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_RTPRECEIVER_H_
