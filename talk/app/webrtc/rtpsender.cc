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
#include "webrtc/base/helpers.h"

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
                                   size_t number_of_channels,
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
                               const std::string& stream_id,
                               AudioProviderInterface* provider,
                               StatsCollector* stats)
    : id_(track->id()),
      stream_id_(stream_id),
      provider_(provider),
      stats_(stats),
      track_(track),
      cached_track_enabled_(track->enabled()),
      sink_adapter_(new LocalAudioSinkAdapter()) {
  RTC_DCHECK(provider != nullptr);
  track_->RegisterObserver(this);
  track_->AddSink(sink_adapter_.get());
}

AudioRtpSender::AudioRtpSender(AudioProviderInterface* provider,
                               StatsCollector* stats)
    : id_(rtc::CreateRandomUuid()),
      stream_id_(rtc::CreateRandomUuid()),
      provider_(provider),
      stats_(stats),
      sink_adapter_(new LocalAudioSinkAdapter()) {}

AudioRtpSender::~AudioRtpSender() {
  Stop();
}

void AudioRtpSender::OnChanged() {
  RTC_DCHECK(!stopped_);
  if (cached_track_enabled_ != track_->enabled()) {
    cached_track_enabled_ = track_->enabled();
    if (can_send_track()) {
      SetAudioSend();
    }
  }
}

bool AudioRtpSender::SetTrack(MediaStreamTrackInterface* track) {
  if (stopped_) {
    LOG(LS_ERROR) << "SetTrack can't be called on a stopped RtpSender.";
    return false;
  }
  if (track && track->kind() != MediaStreamTrackInterface::kAudioKind) {
    LOG(LS_ERROR) << "SetTrack called on audio RtpSender with " << track->kind()
                  << " track.";
    return false;
  }
  AudioTrackInterface* audio_track = static_cast<AudioTrackInterface*>(track);

  // Detach from old track.
  if (track_) {
    track_->RemoveSink(sink_adapter_.get());
    track_->UnregisterObserver(this);
  }

  if (can_send_track() && stats_) {
    stats_->RemoveLocalAudioTrack(track_.get(), ssrc_);
  }

  // Attach to new track.
  bool prev_can_send_track = can_send_track();
  track_ = audio_track;
  if (track_) {
    cached_track_enabled_ = track_->enabled();
    track_->RegisterObserver(this);
    track_->AddSink(sink_adapter_.get());
  }

  // Update audio provider.
  if (can_send_track()) {
    SetAudioSend();
    if (stats_) {
      stats_->AddLocalAudioTrack(track_.get(), ssrc_);
    }
  } else if (prev_can_send_track) {
    cricket::AudioOptions options;
    provider_->SetAudioSend(ssrc_, false, options, nullptr);
  }
  return true;
}

void AudioRtpSender::SetSsrc(uint32_t ssrc) {
  if (stopped_ || ssrc == ssrc_) {
    return;
  }
  // If we are already sending with a particular SSRC, stop sending.
  if (can_send_track()) {
    cricket::AudioOptions options;
    provider_->SetAudioSend(ssrc_, false, options, nullptr);
    if (stats_) {
      stats_->RemoveLocalAudioTrack(track_.get(), ssrc_);
    }
  }
  ssrc_ = ssrc;
  if (can_send_track()) {
    SetAudioSend();
    if (stats_) {
      stats_->AddLocalAudioTrack(track_.get(), ssrc_);
    }
  }
}

void AudioRtpSender::Stop() {
  // TODO(deadbeef): Need to do more here to fully stop sending packets.
  if (stopped_) {
    return;
  }
  if (track_) {
    track_->RemoveSink(sink_adapter_.get());
    track_->UnregisterObserver(this);
  }
  if (can_send_track()) {
    cricket::AudioOptions options;
    provider_->SetAudioSend(ssrc_, false, options, nullptr);
    if (stats_) {
      stats_->RemoveLocalAudioTrack(track_.get(), ssrc_);
    }
  }
  stopped_ = true;
}

void AudioRtpSender::SetAudioSend() {
  RTC_DCHECK(!stopped_ && can_send_track());
  cricket::AudioOptions options;
  if (track_->enabled() && track_->GetSource() &&
      !track_->GetSource()->remote()) {
    // TODO(xians): Remove this static_cast since we should be able to connect
    // a remote audio track to a peer connection.
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
                               const std::string& stream_id,
                               VideoProviderInterface* provider)
    : id_(track->id()),
      stream_id_(stream_id),
      provider_(provider),
      track_(track),
      cached_track_enabled_(track->enabled()) {
  RTC_DCHECK(provider != nullptr);
  track_->RegisterObserver(this);
}

VideoRtpSender::VideoRtpSender(VideoProviderInterface* provider)
    : id_(rtc::CreateRandomUuid()),
      stream_id_(rtc::CreateRandomUuid()),
      provider_(provider) {}

VideoRtpSender::~VideoRtpSender() {
  Stop();
}

void VideoRtpSender::OnChanged() {
  RTC_DCHECK(!stopped_);
  if (cached_track_enabled_ != track_->enabled()) {
    cached_track_enabled_ = track_->enabled();
    if (can_send_track()) {
      SetVideoSend();
    }
  }
}

bool VideoRtpSender::SetTrack(MediaStreamTrackInterface* track) {
  if (stopped_) {
    LOG(LS_ERROR) << "SetTrack can't be called on a stopped RtpSender.";
    return false;
  }
  if (track && track->kind() != MediaStreamTrackInterface::kVideoKind) {
    LOG(LS_ERROR) << "SetTrack called on video RtpSender with " << track->kind()
                  << " track.";
    return false;
  }
  VideoTrackInterface* video_track = static_cast<VideoTrackInterface*>(track);

  // Detach from old track.
  if (track_) {
    track_->UnregisterObserver(this);
  }

  // Attach to new track.
  bool prev_can_send_track = can_send_track();
  track_ = video_track;
  if (track_) {
    cached_track_enabled_ = track_->enabled();
    track_->RegisterObserver(this);
  }

  // Update video provider.
  if (can_send_track()) {
    VideoSourceInterface* source = track_->GetSource();
    // TODO(deadbeef): If SetTrack is called with a disabled track, and the
    // previous track was enabled, this could cause a frame from the new track
    // to slip out. Really, what we need is for SetCaptureDevice and
    // SetVideoSend
    // to be combined into one atomic operation, all the way down to
    // WebRtcVideoSendStream.
    provider_->SetCaptureDevice(ssrc_,
                                source ? source->GetVideoCapturer() : nullptr);
    SetVideoSend();
  } else if (prev_can_send_track) {
    provider_->SetCaptureDevice(ssrc_, nullptr);
    provider_->SetVideoSend(ssrc_, false, nullptr);
  }
  return true;
}

void VideoRtpSender::SetSsrc(uint32_t ssrc) {
  if (stopped_ || ssrc == ssrc_) {
    return;
  }
  // If we are already sending with a particular SSRC, stop sending.
  if (can_send_track()) {
    provider_->SetCaptureDevice(ssrc_, nullptr);
    provider_->SetVideoSend(ssrc_, false, nullptr);
  }
  ssrc_ = ssrc;
  if (can_send_track()) {
    VideoSourceInterface* source = track_->GetSource();
    provider_->SetCaptureDevice(ssrc_,
                                source ? source->GetVideoCapturer() : nullptr);
    SetVideoSend();
  }
}

void VideoRtpSender::Stop() {
  // TODO(deadbeef): Need to do more here to fully stop sending packets.
  if (stopped_) {
    return;
  }
  if (track_) {
    track_->UnregisterObserver(this);
  }
  if (can_send_track()) {
    provider_->SetCaptureDevice(ssrc_, nullptr);
    provider_->SetVideoSend(ssrc_, false, nullptr);
  }
  stopped_ = true;
}

void VideoRtpSender::SetVideoSend() {
  RTC_DCHECK(!stopped_ && can_send_track());
  const cricket::VideoOptions* options = nullptr;
  VideoSourceInterface* source = track_->GetSource();
  if (track_->enabled() && source) {
    options = source->options();
  }
  provider_->SetVideoSend(ssrc_, track_->enabled(), options);
}

}  // namespace webrtc
