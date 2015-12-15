/*
 * libjingle
 * Copyright 2004--2011 Google Inc.
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

#include "talk/app/webrtc/audiotrack.h"

#include "webrtc/base/checks.h"

using rtc::scoped_refptr;

namespace webrtc {

const char MediaStreamTrackInterface::kAudioKind[] = "audio";

// static
scoped_refptr<AudioTrack> AudioTrack::Create(
    const std::string& id,
    const scoped_refptr<AudioSourceInterface>& source) {
  return new rtc::RefCountedObject<AudioTrack>(id, source);
}

AudioTrack::AudioTrack(const std::string& label,
                       const scoped_refptr<AudioSourceInterface>& source)
    : MediaStreamTrack<AudioTrackInterface>(label), audio_source_(source) {
  if (audio_source_) {
    audio_source_->RegisterObserver(this);
    OnChanged();
  }
}

AudioTrack::~AudioTrack() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  set_state(MediaStreamTrackInterface::kEnded);
  if (audio_source_)
    audio_source_->UnregisterObserver(this);
}

std::string AudioTrack::kind() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return kAudioKind;
}

AudioSourceInterface* AudioTrack::GetSource() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return audio_source_.get();
}

void AudioTrack::AddSink(AudioTrackSinkInterface* sink) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (audio_source_)
    audio_source_->AddSink(sink);
}

void AudioTrack::RemoveSink(AudioTrackSinkInterface* sink) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (audio_source_)
    audio_source_->RemoveSink(sink);
}

void AudioTrack::OnChanged() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (state() == kFailed)
    return;  // We can't recover from this state (do we ever set it?).

  TrackState new_state = kInitializing;

  // |audio_source_| must be non-null if we ever get here.
  switch (audio_source_->state()) {
    case MediaSourceInterface::kLive:
    case MediaSourceInterface::kMuted:
      new_state = kLive;
      break;
    case MediaSourceInterface::kEnded:
      new_state = kEnded;
      break;
    case MediaSourceInterface::kInitializing:
    default:
      // use kInitializing.
      break;
  }

  set_state(new_state);
}

}  // namespace webrtc
