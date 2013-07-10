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

#include "talk/session/media/typingmonitor.h"

#include "talk/base/logging.h"
#include "talk/base/thread.h"
#include "talk/session/media/channel.h"

namespace cricket {

TypingMonitor::TypingMonitor(VoiceChannel* channel,
                             talk_base::Thread* worker_thread,
                             const TypingMonitorOptions& settings)
    : channel_(channel),
      worker_thread_(worker_thread),
      mute_period_(settings.mute_period),
      muted_at_(0),
      has_pending_unmute_(false) {
  channel_->media_channel()->SignalMediaError.connect(
      this, &TypingMonitor::OnVoiceChannelError);
  channel_->media_channel()->SetTypingDetectionParameters(
      settings.time_window, settings.cost_per_typing,
      settings.reporting_threshold, settings.penalty_decay,
      settings.type_event_delay);
}

TypingMonitor::~TypingMonitor() {
  // Shortcut any pending unmutes.
  if (has_pending_unmute_) {
    talk_base::MessageList messages;
    worker_thread_->Clear(this, 0, &messages);
    ASSERT(messages.size() == 1);
    channel_->MuteStream(0, false);
    SignalMuted(channel_, false);
  }
}

void TypingMonitor::OnVoiceChannelError(uint32 ssrc,
                                        VoiceMediaChannel::Error error) {
  if (error == VoiceMediaChannel::ERROR_REC_TYPING_NOISE_DETECTED &&
      !channel_->IsStreamMuted(0)) {
    // Please be careful and cognizant about threading issues when editing this
    // code.  The MuteStream() call below is a ::Send and is synchronous as well
    // as the muted signal that comes from this.  This function can be called
    // from any thread.

    // TODO(perkj): Refactor TypingMonitor and the MediaChannel to handle
    // multiple sending audio streams. SSRC 0 means the default sending audio
    // channel.
    channel_->MuteStream(0, true);
    SignalMuted(channel_, true);
    has_pending_unmute_ = true;
    muted_at_ = talk_base::Time();

    worker_thread_->PostDelayed(mute_period_, this, 0);
    LOG(LS_INFO) << "Muting for at least " << mute_period_ << "ms.";
  }
}

/**
 * If we mute due to detected typing and the user also mutes during our waiting
 * period, we don't want to undo their mute.  So, clear our callback.  Should
 * be called on the worker_thread.
 */
void TypingMonitor::OnChannelMuted() {
  if (has_pending_unmute_) {
    talk_base::MessageList removed;
    worker_thread_->Clear(this, 0, &removed);
    ASSERT(removed.size() == 1);
    has_pending_unmute_ = false;
  }
}

/**
 * When the specified mute period has elapsed, unmute, or, if the user kept
 * typing after the initial warning fired, wait for the remainder of time to
 * elapse since they finished and try to unmute again.  Should be called on the
 * worker thread.
 */
void TypingMonitor::OnMessage(talk_base::Message* msg) {
  if (!channel_->IsStreamMuted(0) || !has_pending_unmute_) return;
  int silence_period = channel_->media_channel()->GetTimeSinceLastTyping();
  int expiry_time = mute_period_ - silence_period;
  if (silence_period < 0 || expiry_time < 50) {
    LOG(LS_INFO) << "Mute timeout hit, last typing " << silence_period
                 << "ms ago, unmuting after " << talk_base::TimeSince(muted_at_)
                 << "ms total.";
    has_pending_unmute_ = false;
    channel_->MuteStream(0, false);
    SignalMuted(channel_, false);
  } else {
    LOG(LS_INFO) << "Mute timeout hit, last typing " << silence_period
                 << "ms ago, check again in " << expiry_time << "ms.";
    talk_base::Thread::Current()->PostDelayed(expiry_time, this, 0);
  }
}

}  // namespace cricket
