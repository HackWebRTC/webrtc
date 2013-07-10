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

#include "talk/base/basictypes.h"
#include "talk/base/sigslot.h"

namespace cricket {

class BaseSession;
class Call;
class Session;
struct AudioInfo;
struct MediaStreams;

// Note that the call's audio monitor must be started before this is started.
// It's recommended that the audio monitor be started with a 100 ms period.
class CurrentSpeakerMonitor : public sigslot::has_slots<> {
 public:
  CurrentSpeakerMonitor(Call* call, BaseSession* session);
  ~CurrentSpeakerMonitor();

  BaseSession* session() const { return session_; }

  void Start();
  void Stop();

  // Used by tests.  Note that the actual minimum time between switches
  // enforced by the monitor will be the given value plus or minus the
  // resolution of the system clock.
  void set_min_time_between_switches(uint32 min_time_between_switches);

  // This is fired when the current speaker changes, and provides his audio
  // SSRC.  This only fires after the audio monitor on the underlying Call has
  // been started.
  sigslot::signal2<CurrentSpeakerMonitor*, uint32> SignalUpdate;

 private:
  void OnAudioMonitor(Call* call, const AudioInfo& info);
  void OnMediaStreamsUpdate(Call* call,
                            Session* session,
                            const MediaStreams& added,
                            const MediaStreams& removed);

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
  Call* call_;
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
