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

#include "webrtc/api/mediastreamtrackproxy.h"
#include "webrtc/api/audiotrack.h"
#include "webrtc/api/videosourceproxy.h"
#include "webrtc/api/videotrack.h"

namespace webrtc {

AudioRtpReceiver::AudioRtpReceiver(MediaStreamInterface* stream,
                                   const std::string& track_id,
                                   uint32_t ssrc,
                                   AudioProviderInterface* provider)
    : id_(track_id),
      ssrc_(ssrc),
      provider_(provider),
      track_(AudioTrackProxy::Create(
          rtc::Thread::Current(),
          AudioTrack::Create(track_id,
                             RemoteAudioSource::Create(ssrc, provider)))),
      cached_track_enabled_(track_->enabled()) {
  RTC_DCHECK(track_->GetSource()->remote());
  track_->RegisterObserver(this);
  track_->GetSource()->RegisterAudioObserver(this);
  Reconfigure();
  stream->AddTrack(track_);
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

VideoRtpReceiver::VideoRtpReceiver(MediaStreamInterface* stream,
                                   const std::string& track_id,
                                   rtc::Thread* worker_thread,
                                   uint32_t ssrc,
                                   VideoProviderInterface* provider)
    : id_(track_id),
      ssrc_(ssrc),
      provider_(provider),
      source_(new RefCountedObject<VideoTrackSource>(&broadcaster_,
                                                     true /* remote */)),
      track_(VideoTrackProxy::Create(
          rtc::Thread::Current(),
          worker_thread,
          VideoTrack::Create(
              track_id,
              VideoTrackSourceProxy::Create(rtc::Thread::Current(),
                                            worker_thread,
                                            source_)))) {
  source_->SetState(MediaSourceInterface::kLive);
  provider_->SetVideoPlayout(ssrc_, true, &broadcaster_);
  stream->AddTrack(track_);
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
  source_->SetState(MediaSourceInterface::kEnded);
  source_->OnSourceDestroyed();
  provider_->SetVideoPlayout(ssrc_, false, nullptr);
  provider_ = nullptr;
}

}  // namespace webrtc
