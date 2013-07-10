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

#ifndef TALK_SESSION_MEDIA_SOUNDCLIP_H_
#define TALK_SESSION_MEDIA_SOUNDCLIP_H_

#include "talk/base/scoped_ptr.h"
#include "talk/media/base/mediaengine.h"

namespace talk_base {

class Thread;

}

namespace cricket {

// Soundclip wraps SoundclipMedia to support marshalling calls to the proper
// thread.
class Soundclip : private talk_base::MessageHandler {
 public:
  Soundclip(talk_base::Thread* thread, SoundclipMedia* soundclip_media);

  // Plays a sound out to the speakers with the given audio stream. The stream
  // must be 16-bit little-endian 16 kHz PCM. If a stream is already playing
  // on this Soundclip, it is stopped. If clip is NULL, nothing is played.
  // Returns whether it was successful.
  bool PlaySound(const void* clip,
                 int len,
                 SoundclipMedia::SoundclipFlags flags);

 private:
  bool PlaySound_w(const void* clip,
                   int len,
                   SoundclipMedia::SoundclipFlags flags);

  // From MessageHandler
  virtual void OnMessage(talk_base::Message* message);

  talk_base::Thread* worker_thread_;
  talk_base::scoped_ptr<SoundclipMedia> soundclip_media_;
};

}  // namespace cricket

#endif  // TALK_SESSION_MEDIA_SOUNDCLIP_H_
