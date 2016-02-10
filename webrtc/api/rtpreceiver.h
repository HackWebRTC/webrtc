/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains classes that implement RtpReceiverInterface.
// An RtpReceiver associates a MediaStreamTrackInterface with an underlying
// transport (provided by AudioProviderInterface/VideoProviderInterface)

#ifndef WEBRTC_API_RTPRECEIVER_H_
#define WEBRTC_API_RTPRECEIVER_H_

#include <string>

#include "webrtc/api/mediastreamprovider.h"
#include "webrtc/api/rtpreceiverinterface.h"
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

  const std::string id_;
  const rtc::scoped_refptr<AudioTrackInterface> track_;
  const uint32_t ssrc_;
  AudioProviderInterface* provider_;  // Set to null in Stop().
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

#endif  // WEBRTC_API_RTPRECEIVER_H_
