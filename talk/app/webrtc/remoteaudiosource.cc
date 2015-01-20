/*
 * libjingle
 * Copyright 2014 Google Inc.
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

#include "talk/app/webrtc/remoteaudiosource.h"

#include <algorithm>
#include <functional>

#include "webrtc/base/logging.h"

namespace webrtc {

rtc::scoped_refptr<RemoteAudioSource> RemoteAudioSource::Create() {
  return new rtc::RefCountedObject<RemoteAudioSource>();
}

RemoteAudioSource::RemoteAudioSource() {
}

RemoteAudioSource::~RemoteAudioSource() {
  ASSERT(audio_observers_.empty());
}

MediaSourceInterface::SourceState RemoteAudioSource::state() const {
  return MediaSourceInterface::kLive;
}

void RemoteAudioSource::SetVolume(double volume) {
  ASSERT(volume >= 0 && volume <= 10);
  for (AudioObserverList::iterator it = audio_observers_.begin();
       it != audio_observers_.end(); ++it) {
    (*it)->OnSetVolume(volume);
  }
}

void RemoteAudioSource::RegisterAudioObserver(AudioObserver* observer) {
  ASSERT(observer != NULL);
  ASSERT(std::find(audio_observers_.begin(), audio_observers_.end(),
                   observer) == audio_observers_.end());
  audio_observers_.push_back(observer);
}

void RemoteAudioSource::UnregisterAudioObserver(AudioObserver* observer) {
  ASSERT(observer != NULL);
  audio_observers_.remove(observer);
}

}  // namespace webrtc
