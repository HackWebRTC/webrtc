/*
 * libjingle
 * Copyright 2011 Google Inc.
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

// CurrentSpeakerMonitor monitors the audio levels for a session and determines
// which participant is currently speaking.

#ifndef TALK_SESSION_MEDIA_CURRENTSPEAKERMONITOR_H_
#define TALK_SESSION_MEDIA_CURRENTSPEAKERMONITOR_H_

#include <map>

#include "webrtc/base/basictypes.h"
#include "webrtc/base/sigslot.h"

namespace cricket {

class BaseSession;
struct AudioInfo;
struct MediaStreams;

class AudioSourceContext {
 public:
  sigslot::signal2<AudioSourceContext*, const cricket::AudioInfo&>
      SignalAudioMonitor;
  sigslot::signal2<AudioSourceContext*, cricket::BaseSession*>
      SignalMediaStreamsReset;
  sigslot::signal4<AudioSourceContext*, cricket::BaseSession*,
      const cricket::MediaStreams&, const cricket::MediaStreams&>
          SignalMediaStreamsUpdate;
};

// CurrentSpeakerMonitor can be used to monitor the audio-levels from
// many audio-sources and report on changes in the loudest audio-source.
// Its a generic type and relies on an AudioSourceContext which is aware of
// the audio-sources. AudioSourceContext needs to provide two signals namely
// SignalAudioInfoMonitor - provides audio info of the all current speakers.
// SignalMediaSourcesUpdated - provides updates when a speaker leaves or joins.
// Note that the AudioSourceContext's audio monitor must be started
// before this is started.
// It's recommended that the audio monitor be started with a 100 ms period.
class CurrentSpeakerMonitor : public sigslot::has_slots<> {
 public:
  CurrentSpeakerMonitor(AudioSourceContext* audio_source_context,
                        BaseSession* session);
  ~CurrentSpeakerMonitor();

  BaseSession* session() const { return session_; }

  void Start();
  void Stop();

  // Used by tests.  Note that the actual minimum time between switches
  // enforced by the monitor will be the given value plus or minus the
  // resolution of the system clock.
  void set_min_time_between_switches(uint32 min_time_between_switches);

  // This is fired when the current speaker changes, and provides his audio
  // SSRC.  This only fires after the audio monitor on the underlying
  // AudioSourceContext has been started.
  sigslot::signal2<CurrentSpeakerMonitor*, uint32> SignalUpdate;

 private:
  void OnAudioMonitor(AudioSourceContext* audio_source_context,
                      const AudioInfo& info);
  void OnMediaStreamsUpdate(AudioSourceContext* audio_source_context,
                            BaseSession* session,
                            const MediaStreams& added,
                            const MediaStreams& removed);
  void OnMediaStreamsReset(AudioSourceContext* audio_source_context,
                           BaseSession* session);

  // These are states that a participant will pass through so that we gradually
  // recognize that they have started and stopped speaking.  This avoids
  // "twitchiness".
  enum SpeakingState {
    SS_NOT_SPEAKING,
    SS_MIGHT_BE_SPEAKING,
    SS_SPEAKING,
    SS_WAS_SPEAKING_RECENTLY1,
    SS_WAS_SPEAKING_RECENTLY2
  };

  bool started_;
  AudioSourceContext* audio_source_context_;
  BaseSession* session_;
  std::map<uint32, SpeakingState> ssrc_to_speaking_state_map_;
  uint32 current_speaker_ssrc_;
  // To prevent overswitching, switching is disabled for some time after a
  // switch is made.  This gives us the earliest time a switch is permitted.
  uint32 earliest_permitted_switch_time_;
  uint32 min_time_between_switches_;
};

}

#endif  // TALK_SESSION_MEDIA_CURRENTSPEAKERMONITOR_H_
