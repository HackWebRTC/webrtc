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

#ifndef TALK_APP_WEBRTC_REMOTEAUDIOSOURCE_H_
#define TALK_APP_WEBRTC_REMOTEAUDIOSOURCE_H_

#include <list>
#include <string>

#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/app/webrtc/notifier.h"
#include "talk/media/base/audiorenderer.h"
#include "webrtc/audio/audio_sink.h"
#include "webrtc/base/criticalsection.h"

namespace rtc {
struct Message;
class Thread;
}  // namespace rtc

namespace webrtc {

class AudioProviderInterface;

// This class implements the audio source used by the remote audio track.
class RemoteAudioSource : public Notifier<AudioSourceInterface> {
 public:
  // Creates an instance of RemoteAudioSource.
  static rtc::scoped_refptr<RemoteAudioSource> Create(
      uint32_t ssrc,
      AudioProviderInterface* provider);

  // MediaSourceInterface implementation.
  MediaSourceInterface::SourceState state() const override;
  bool remote() const override;

  void AddSink(AudioTrackSinkInterface* sink) override;
  void RemoveSink(AudioTrackSinkInterface* sink) override;

 protected:
  RemoteAudioSource();
  ~RemoteAudioSource() override;

  // Post construction initialize where we can do things like save a reference
  // to ourselves (need to be fully constructed).
  void Initialize(uint32_t ssrc, AudioProviderInterface* provider);

 private:
  typedef std::list<AudioObserver*> AudioObserverList;

  // AudioSourceInterface implementation.
  void SetVolume(double volume) override;
  void RegisterAudioObserver(AudioObserver* observer) override;
  void UnregisterAudioObserver(AudioObserver* observer) override;

  class Sink;
  void OnData(const AudioSinkInterface::Data& audio);
  void OnAudioProviderGone();

  class MessageHandler;
  void OnMessage(rtc::Message* msg);

  AudioObserverList audio_observers_;
  rtc::CriticalSection sink_lock_;
  std::list<AudioTrackSinkInterface*> sinks_;
  rtc::Thread* const main_thread_;
  SourceState state_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_REMOTEAUDIOSOURCE_H_
