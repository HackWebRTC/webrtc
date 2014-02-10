/*
 * libjingle
 * Copyright 2005 Google Inc.
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

// Class to collect statistics from a media channel

#ifndef TALK_SESSION_MEDIA_MEDIAMONITOR_H_
#define TALK_SESSION_MEDIA_MEDIAMONITOR_H_

#include "talk/base/criticalsection.h"
#include "talk/base/sigslot.h"
#include "talk/base/thread.h"
#include "talk/media/base/mediachannel.h"

namespace cricket {

// The base MediaMonitor class, independent of voice and video.
class MediaMonitor : public talk_base::MessageHandler,
    public sigslot::has_slots<> {
 public:
  MediaMonitor(talk_base::Thread* worker_thread,
               talk_base::Thread* monitor_thread);
  ~MediaMonitor();

  void Start(uint32 milliseconds);
  void Stop();

 protected:
  void OnMessage(talk_base::Message *message);
  void PollMediaChannel();
  virtual void GetStats() = 0;
  virtual void Update() = 0;

  talk_base::CriticalSection crit_;
  talk_base::Thread* worker_thread_;
  talk_base::Thread* monitor_thread_;
  bool monitoring_;
  uint32 rate_;
};

// Templatized MediaMonitor that can deal with different kinds of media.
template<class MC, class MI>
class MediaMonitorT : public MediaMonitor {
 public:
  MediaMonitorT(MC* media_channel, talk_base::Thread* worker_thread,
                talk_base::Thread* monitor_thread)
      : MediaMonitor(worker_thread, monitor_thread),
        media_channel_(media_channel) {}
  sigslot::signal2<MC*, const MI&> SignalUpdate;

 protected:
  // These routines assume the crit_ lock is held by the calling thread.
  virtual void GetStats() {
    media_info_.Clear();
    media_channel_->GetStats(&media_info_);
  }
  virtual void Update() {
    MI stats(media_info_);
    crit_.Leave();
    SignalUpdate(media_channel_, stats);
    crit_.Enter();
  }

 private:
  MC* media_channel_;
  MI media_info_;
};

typedef MediaMonitorT<VoiceMediaChannel, VoiceMediaInfo> VoiceMediaMonitor;
typedef MediaMonitorT<VideoMediaChannel, VideoMediaInfo> VideoMediaMonitor;
typedef MediaMonitorT<DataMediaChannel, DataMediaInfo> DataMediaMonitor;

}  // namespace cricket

#endif  // TALK_SESSION_MEDIA_MEDIAMONITOR_H_
