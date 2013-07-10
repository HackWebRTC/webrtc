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

#include "talk/session/media/soundclip.h"

namespace cricket {

enum {
  MSG_PLAYSOUND = 1,
};

struct PlaySoundMessageData : talk_base::MessageData {
  PlaySoundMessageData(const void *c,
                       int l,
                       SoundclipMedia::SoundclipFlags f)
      : clip(c),
        len(l),
        flags(f),
        result(false) {
  }

  const void *clip;
  int len;
  SoundclipMedia::SoundclipFlags flags;
  bool result;
};

Soundclip::Soundclip(talk_base::Thread *thread, SoundclipMedia *soundclip_media)
    : worker_thread_(thread),
      soundclip_media_(soundclip_media) {
}

bool Soundclip::PlaySound(const void *clip,
                          int len,
                          SoundclipMedia::SoundclipFlags flags) {
  PlaySoundMessageData data(clip, len, flags);
  worker_thread_->Send(this, MSG_PLAYSOUND, &data);
  return data.result;
}

bool Soundclip::PlaySound_w(const void *clip,
                            int len,
                            SoundclipMedia::SoundclipFlags flags) {
  return soundclip_media_->PlaySound(static_cast<const char *>(clip),
                                     len,
                                     flags);
}

void Soundclip::OnMessage(talk_base::Message *message) {
  ASSERT(message->message_id == MSG_PLAYSOUND);
  PlaySoundMessageData *data =
      static_cast<PlaySoundMessageData *>(message->pdata);
  data->result = PlaySound_w(data->clip,
                             data->len,
                             data->flags);
}

}  // namespace cricket
