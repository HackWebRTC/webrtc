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

#include "talk/app/webrtc/remoteaudiotrack.h"

#include "talk/app/webrtc/remoteaudiosource.h"

using rtc::scoped_refptr;

namespace webrtc {

// static
scoped_refptr<RemoteAudioTrack> RemoteAudioTrack::Create(
    const std::string& id,
    const scoped_refptr<RemoteAudioSource>& source) {
  return new rtc::RefCountedObject<RemoteAudioTrack>(id, source);
}

RemoteAudioTrack::RemoteAudioTrack(
    const std::string& label,
    const scoped_refptr<RemoteAudioSource>& source)
    : MediaStreamTrack<AudioTrackInterface>(label), audio_source_(source) {
  audio_source_->RegisterObserver(this);
  TrackState new_state = kInitializing;
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
      // kInitializing;
      break;
  }
  set_state(new_state);
}

RemoteAudioTrack::~RemoteAudioTrack() {
  set_state(MediaStreamTrackInterface::kEnded);
  audio_source_->UnregisterObserver(this);
}

std::string RemoteAudioTrack::kind() const {
  return MediaStreamTrackInterface::kAudioKind;
}

AudioSourceInterface* RemoteAudioTrack::GetSource() const {
  return audio_source_.get();
}

void RemoteAudioTrack::AddSink(AudioTrackSinkInterface* sink) {
  audio_source_->AddSink(sink);
}

void RemoteAudioTrack::RemoveSink(AudioTrackSinkInterface* sink) {
  audio_source_->RemoveSink(sink);
}

bool RemoteAudioTrack::GetSignalLevel(int* level) {
  return false;
}

void RemoteAudioTrack::OnChanged() {
  if (audio_source_->state() == MediaSourceInterface::kEnded)
    set_state(MediaStreamTrackInterface::kEnded);
}

}  // namespace webrtc
