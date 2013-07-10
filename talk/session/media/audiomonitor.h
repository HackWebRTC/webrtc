/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#ifndef TALK_SESSION_MEDIA_AUDIOMONITOR_H_
#define TALK_SESSION_MEDIA_AUDIOMONITOR_H_

#include "talk/base/sigslot.h"
#include "talk/base/thread.h"
#include "talk/p2p/base/port.h"
#include <vector>

namespace cricket {

class VoiceChannel;

struct AudioInfo {
  int input_level;
  int output_level;
  typedef std::vector<std::pair<uint32, int> > StreamList;
  StreamList active_streams; // ssrcs contributing to output_level
};

class AudioMonitor : public talk_base::MessageHandler,
    public sigslot::has_slots<> {
 public:
  AudioMonitor(VoiceChannel* voice_channel, talk_base::Thread *monitor_thread);
  ~AudioMonitor();

  void Start(int cms);
  void Stop();

  VoiceChannel* voice_channel();
  talk_base::Thread *monitor_thread();

  sigslot::signal2<AudioMonitor*, const AudioInfo&> SignalUpdate;

 protected:
  void OnMessage(talk_base::Message *message);
  void PollVoiceChannel();

  AudioInfo audio_info_;
  VoiceChannel* voice_channel_;
  talk_base::Thread* monitoring_thread_;
  talk_base::CriticalSection crit_;
  uint32 rate_;
  bool monitoring_;
};

}

#endif  // TALK_SESSION_MEDIA_AUDIOMONITOR_H_
