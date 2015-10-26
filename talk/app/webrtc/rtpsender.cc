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

#include "talk/app/webrtc/rtpsender.h"

#include "talk/app/webrtc/localaudiosource.h"
#include "talk/app/webrtc/videosourceinterface.h"

namespace webrtc {

LocalAudioSinkAdapter::LocalAudioSinkAdapter() : sink_(nullptr) {}

LocalAudioSinkAdapter::~LocalAudioSinkAdapter() {
  rtc::CritScope lock(&lock_);
  if (sink_)
    sink_->OnClose();
}

void LocalAudioSinkAdapter::OnData(const void* audio_data,
                                   int bits_per_sample,
                                   int sample_rate,
                                   int number_of_channels,
                                   size_t number_of_frames) {
  rtc::CritScope lock(&lock_);
  if (sink_) {
    sink_->OnData(audio_data, bits_per_sample, sample_rate, number_of_channels,
                  number_of_frames);
  }
}

void LocalAudioSinkAdapter::SetSink(cricket::AudioRenderer::Sink* sink) {
  rtc::CritScope lock(&lock_);
  ASSERT(!sink || !sink_);
  sink_ = sink;
}

AudioRtpSender::AudioRtpSender(AudioTrackInterface* track,
                               uint32_t ssrc,
                               AudioProviderInterface* provider)
    : id_(track->id()),
      track_(track),
      ssrc_(ssrc),
      provider_(provider),
      cached_track_enabled_(track->enabled()),
      sink_adapter_(new LocalAudioSinkAdapter()) {
  track_->RegisterObserver(this);
  track_->AddSink(sink_adapter_.get());
  Reconfigure();
}

AudioRtpSender::~AudioRtpSender() {
  track_->RemoveSink(sink_adapter_.get());
  track_->UnregisterObserver(this);
  Stop();
}

void AudioRtpSender::OnChanged() {
  if (cached_track_enabled_ != track_->enabled()) {
    cached_track_enabled_ = track_->enabled();
    Reconfigure();
  }
}

bool AudioRtpSender::SetTrack(MediaStreamTrackInterface* track) {
  if (track->kind() != "audio") {
    LOG(LS_ERROR) << "SetTrack called on audio RtpSender with " << track->kind()
                  << " track.";
    return false;
  }
  AudioTrackInterface* audio_track = static_cast<AudioTrackInterface*>(track);

  // Detach from old track.
  track_->RemoveSink(sink_adapter_.get());
  track_->UnregisterObserver(this);

  // Attach to new track.
  track_ = audio_track;
  cached_track_enabled_ = track_->enabled();
  track_->RegisterObserver(this);
  track_->AddSink(sink_adapter_.get());
  Reconfigure();
  return true;
}

void AudioRtpSender::Stop() {
  // TODO(deadbeef): Need to do more here to fully stop sending packets.
  if (!provider_) {
    return;
  }
  cricket::AudioOptions options;
  provider_->SetAudioSend(ssrc_, false, options, nullptr);
  provider_ = nullptr;
}

void AudioRtpSender::Reconfigure() {
  if (!provider_) {
    return;
  }
  cricket::AudioOptions options;
  if (track_->enabled() && track_->GetSource()) {
    // TODO(xians): Remove this static_cast since we should be able to connect
    // a remote audio track to peer connection.
    options = static_cast<LocalAudioSource*>(track_->GetSource())->options();
  }

  // Use the renderer if the audio track has one, otherwise use the sink
  // adapter owned by this class.
  cricket::AudioRenderer* renderer =
      track_->GetRenderer() ? track_->GetRenderer() : sink_adapter_.get();
  ASSERT(renderer != nullptr);
  provider_->SetAudioSend(ssrc_, track_->enabled(), options, renderer);
}

VideoRtpSender::VideoRtpSender(VideoTrackInterface* track,
                               uint32_t ssrc,
                               VideoProviderInterface* provider)
    : id_(track->id()),
      track_(track),
      ssrc_(ssrc),
      provider_(provider),
      cached_track_enabled_(track->enabled()) {
  track_->RegisterObserver(this);
  VideoSourceInterface* source = track_->GetSource();
  if (source) {
    provider_->SetCaptureDevice(ssrc_, source->GetVideoCapturer());
  }
  Reconfigure();
}

VideoRtpSender::~VideoRtpSender() {
  track_->UnregisterObserver(this);
  Stop();
}

void VideoRtpSender::OnChanged() {
  if (cached_track_enabled_ != track_->enabled()) {
    cached_track_enabled_ = track_->enabled();
    Reconfigure();
  }
}

bool VideoRtpSender::SetTrack(MediaStreamTrackInterface* track) {
  if (track->kind() != "video") {
    LOG(LS_ERROR) << "SetTrack called on video RtpSender with " << track->kind()
                  << " track.";
    return false;
  }
  VideoTrackInterface* video_track = static_cast<VideoTrackInterface*>(track);

  // Detach from old track.
  track_->UnregisterObserver(this);

  // Attach to new track.
  track_ = video_track;
  cached_track_enabled_ = track_->enabled();
  track_->RegisterObserver(this);
  Reconfigure();
  return true;
}

void VideoRtpSender::Stop() {
  // TODO(deadbeef): Need to do more here to fully stop sending packets.
  if (!provider_) {
    return;
  }
  provider_->SetCaptureDevice(ssrc_, nullptr);
  provider_->SetVideoSend(ssrc_, false, nullptr);
  provider_ = nullptr;
}

void VideoRtpSender::Reconfigure() {
  if (!provider_) {
    return;
  }
  const cricket::VideoOptions* options = nullptr;
  VideoSourceInterface* source = track_->GetSource();
  if (track_->enabled() && source) {
    options = source->options();
  }
  provider_->SetVideoSend(ssrc_, track_->enabled(), options);
}

}  // namespace webrtc
