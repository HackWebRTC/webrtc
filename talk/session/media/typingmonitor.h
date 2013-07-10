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

#ifndef TALK_SESSION_MEDIA_TYPINGMONITOR_H_
#define TALK_SESSION_MEDIA_TYPINGMONITOR_H_

#include "talk/base/messagehandler.h"
#include "talk/media/base/mediachannel.h"

namespace talk_base {
class Thread;
}

namespace cricket {

class VoiceChannel;
class BaseChannel;

struct TypingMonitorOptions {
  int cost_per_typing;
  int mute_period;
  int penalty_decay;
  int reporting_threshold;
  int time_window;
  int type_event_delay;
  size_t min_participants;
};

/**
 * An object that observes a channel and listens for typing detection warnings,
 * which can be configured to mute audio capture of that channel for some period
 * of time.  The purpose is to automatically mute someone if they are disturbing
 * a conference with loud keystroke audio signals.
 */
class TypingMonitor
    : public talk_base::MessageHandler, public sigslot::has_slots<> {
 public:
  TypingMonitor(VoiceChannel* channel, talk_base::Thread* worker_thread,
                const TypingMonitorOptions& params);
  ~TypingMonitor();

  sigslot::signal2<BaseChannel*, bool> SignalMuted;

  void OnChannelMuted();

 private:
  void OnVoiceChannelError(uint32 ssrc, VoiceMediaChannel::Error error);
  void OnMessage(talk_base::Message* msg);

  VoiceChannel* channel_;
  talk_base::Thread* worker_thread_;
  int mute_period_;
  int muted_at_;
  bool has_pending_unmute_;
};

}  // namespace cricket

#endif  // TALK_SESSION_MEDIA_TYPINGMONITOR_H_

