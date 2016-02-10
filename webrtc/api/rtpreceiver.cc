/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/rtpreceiver.h"

#include "webrtc/api/videosourceinterface.h"

namespace webrtc {

AudioRtpReceiver::AudioRtpReceiver(AudioTrackInterface* track,
                                   uint32_t ssrc,
                                   AudioProviderInterface* provider)
    : id_(track->id()),
      track_(track),
      ssrc_(ssrc),
      provider_(provider),
      cached_track_enabled_(track->enabled()) {
  RTC_DCHECK(track_->GetSource()->remote());
  track_->RegisterObserver(this);
  track_->GetSource()->RegisterAudioObserver(this);
  Reconfigure();
}

AudioRtpReceiver::~AudioRtpReceiver() {
  track_->GetSource()->UnregisterAudioObserver(this);
  track_->UnregisterObserver(this);
  Stop();
}

void AudioRtpReceiver::OnChanged() {
  if (cached_track_enabled_ != track_->enabled()) {
    cached_track_enabled_ = track_->enabled();
    Reconfigure();
  }
}

void AudioRtpReceiver::OnSetVolume(double volume) {
  // When the track is disabled, the volume of the source, which is the
  // corresponding WebRtc Voice Engine channel will be 0. So we do not allow
  // setting the volume to the source when the track is disabled.
  if (provider_ && track_->enabled())
    provider_->SetAudioPlayoutVolume(ssrc_, volume);
}

void AudioRtpReceiver::Stop() {
  // TODO(deadbeef): Need to do more here to fully stop receiving packets.
  if (!provider_) {
    return;
  }
  provider_->SetAudioPlayout(ssrc_, false);
  provider_ = nullptr;
}

void AudioRtpReceiver::Reconfigure() {
  if (!provider_) {
    return;
  }
  provider_->SetAudioPlayout(ssrc_, track_->enabled());
}

VideoRtpReceiver::VideoRtpReceiver(VideoTrackInterface* track,
                                   uint32_t ssrc,
                                   VideoProviderInterface* provider)
    : id_(track->id()), track_(track), ssrc_(ssrc), provider_(provider) {
  provider_->SetVideoPlayout(ssrc_, true, track_->GetSink());
}

VideoRtpReceiver::~VideoRtpReceiver() {
  // Since cricket::VideoRenderer is not reference counted,
  // we need to remove it from the provider before we are deleted.
  Stop();
}

void VideoRtpReceiver::Stop() {
  // TODO(deadbeef): Need to do more here to fully stop receiving packets.
  if (!provider_) {
    return;
  }
  provider_->SetVideoPlayout(ssrc_, false, nullptr);
  provider_ = nullptr;
}

}  // namespace webrtc
