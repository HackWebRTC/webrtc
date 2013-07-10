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

#include "talk/session/media/audiomonitor.h"
#include "talk/session/media/voicechannel.h"
#include <cassert>

namespace cricket {

const uint32 MSG_MONITOR_POLL = 1;
const uint32 MSG_MONITOR_START = 2;
const uint32 MSG_MONITOR_STOP = 3;
const uint32 MSG_MONITOR_SIGNAL = 4;

AudioMonitor::AudioMonitor(VoiceChannel *voice_channel,
                           talk_base::Thread *monitor_thread) {
  voice_channel_ = voice_channel;
  monitoring_thread_ = monitor_thread;
  monitoring_ = false;
}

AudioMonitor::~AudioMonitor() {
  voice_channel_->worker_thread()->Clear(this);
  monitoring_thread_->Clear(this);
}

void AudioMonitor::Start(int milliseconds) {
  rate_ = milliseconds;
  if (rate_ < 100)
    rate_ = 100;
  voice_channel_->worker_thread()->Post(this, MSG_MONITOR_START);
}

void AudioMonitor::Stop() {
  voice_channel_->worker_thread()->Post(this, MSG_MONITOR_STOP);
}

void AudioMonitor::OnMessage(talk_base::Message *message) {
  talk_base::CritScope cs(&crit_);

  switch (message->message_id) {
  case MSG_MONITOR_START:
    assert(talk_base::Thread::Current() == voice_channel_->worker_thread());
    if (!monitoring_) {
      monitoring_ = true;
      PollVoiceChannel();
    }
    break;

  case MSG_MONITOR_STOP:
    assert(talk_base::Thread::Current() == voice_channel_->worker_thread());
    if (monitoring_) {
      monitoring_ = false;
      voice_channel_->worker_thread()->Clear(this);
    }
    break;

  case MSG_MONITOR_POLL:
    assert(talk_base::Thread::Current() == voice_channel_->worker_thread());
    PollVoiceChannel();
    break;

  case MSG_MONITOR_SIGNAL:
    {
      assert(talk_base::Thread::Current() == monitoring_thread_);
      AudioInfo info = audio_info_;
      crit_.Leave();
      SignalUpdate(this, info);
      crit_.Enter();
    }
    break;
  }
}

void AudioMonitor::PollVoiceChannel() {
  talk_base::CritScope cs(&crit_);
  assert(talk_base::Thread::Current() == voice_channel_->worker_thread());

  // Gather connection infos
  audio_info_.input_level = voice_channel_->GetInputLevel_w();
  audio_info_.output_level = voice_channel_->GetOutputLevel_w();
  voice_channel_->GetActiveStreams_w(&audio_info_.active_streams);

  // Signal the monitoring thread, start another poll timer
  monitoring_thread_->Post(this, MSG_MONITOR_SIGNAL);
  voice_channel_->worker_thread()->PostDelayed(rate_, this, MSG_MONITOR_POLL);
}

VoiceChannel *AudioMonitor::voice_channel() {
  return voice_channel_;
}

talk_base::Thread *AudioMonitor::monitor_thread() {
  return monitoring_thread_;
}

}
