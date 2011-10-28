/*
 * libjingle
 * Copyright 2011, Google Inc.
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

#include "talk/app/webrtc_dev/mediastreamimpl.h"
#include "talk/base/logging.h"

namespace webrtc {

talk_base::scoped_refptr<MediaStream> MediaStream::Create(
    const std::string& label) {
  talk_base::RefCountedObject<MediaStream>* stream =
      new talk_base::RefCountedObject<MediaStream>(label);
  return stream;
}

MediaStream::MediaStream(const std::string& label)
    : label_(label),
      ready_state_(MediaStreamInterface::kInitializing),
      audio_track_list_(
          new talk_base::RefCountedObject<
          MediaStreamTrackList<AudioTrackInterface> >()),
      video_track_list_(
          new talk_base::RefCountedObject<
          MediaStreamTrackList<VideoTrackInterface> >()) {
}

void MediaStream::set_ready_state(
    MediaStreamInterface::ReadyState new_state) {
  if (ready_state_ != new_state) {
    ready_state_ = new_state;
    Notifier<LocalMediaStreamInterface>::FireOnChanged();
  }
}

bool MediaStream::AddTrack(AudioTrackInterface* track) {
  if (ready_state() != kInitializing)
    return false;
  audio_track_list_->AddTrack(track);
  return true;
}

bool MediaStream::AddTrack(VideoTrackInterface* track) {
  if (ready_state() != kInitializing)
    return false;
  video_track_list_->AddTrack(track);
  return true;
}

}  // namespace webrtc
