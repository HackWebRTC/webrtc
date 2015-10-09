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

#include "talk/app/webrtc/rtpreceiver.h"

#include "talk/app/webrtc/videosourceinterface.h"

namespace webrtc {

AudioRtpReceiver::AudioRtpReceiver(AudioTrackInterface* track,
                                   uint32_t ssrc,
                                   AudioProviderInterface* provider)
    : id_(track->id()),
      track_(track),
      ssrc_(ssrc),
      provider_(provider),
      cached_track_enabled_(track->enabled()) {
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
  provider_->SetVideoPlayout(ssrc_, true, track_->GetSource()->FrameInput());
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
