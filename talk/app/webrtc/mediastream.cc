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

#include "talk/app/webrtc/mediastream.h"
#include "talk/base/logging.h"

namespace webrtc {

template <class V>
static typename V::iterator FindTrack(V* vector,
                                      const std::string& track_id) {
  typename V::iterator it = vector->begin();
  for (; it != vector->end(); ++it) {
    if ((*it)->id() == track_id) {
      break;
    }
  }
  return it;
};

talk_base::scoped_refptr<MediaStream> MediaStream::Create(
    const std::string& label) {
  talk_base::RefCountedObject<MediaStream>* stream =
      new talk_base::RefCountedObject<MediaStream>(label);
  return stream;
}

MediaStream::MediaStream(const std::string& label)
    : label_(label) {
}

bool MediaStream::AddTrack(AudioTrackInterface* track) {
  return AddTrack<AudioTrackVector, AudioTrackInterface>(&audio_tracks_, track);
}

bool MediaStream::AddTrack(VideoTrackInterface* track) {
  return AddTrack<VideoTrackVector, VideoTrackInterface>(&video_tracks_, track);
}

bool MediaStream::RemoveTrack(AudioTrackInterface* track) {
  return RemoveTrack<AudioTrackVector>(&audio_tracks_, track);
}

bool MediaStream::RemoveTrack(VideoTrackInterface* track) {
  return RemoveTrack<VideoTrackVector>(&video_tracks_, track);
}

talk_base::scoped_refptr<AudioTrackInterface>
MediaStream::FindAudioTrack(const std::string& track_id) {
  AudioTrackVector::iterator it = FindTrack(&audio_tracks_, track_id);
  if (it == audio_tracks_.end())
    return NULL;
  return *it;
}

talk_base::scoped_refptr<VideoTrackInterface>
MediaStream::FindVideoTrack(const std::string& track_id) {
  VideoTrackVector::iterator it = FindTrack(&video_tracks_, track_id);
  if (it == video_tracks_.end())
    return NULL;
  return *it;
}

template <typename TrackVector, typename Track>
bool MediaStream::AddTrack(TrackVector* tracks, Track* track) {
  typename TrackVector::iterator it = FindTrack(tracks, track->id());
  if (it != tracks->end())
    return false;
  tracks->push_back(track);
  FireOnChanged();
  return true;
}

template <typename TrackVector>
bool MediaStream::RemoveTrack(TrackVector* tracks,
                              MediaStreamTrackInterface* track) {
  ASSERT(tracks != NULL);
  if (!track)
    return false;
  typename TrackVector::iterator it = FindTrack(tracks, track->id());
  if (it == tracks->end())
    return false;
  tracks->erase(it);
  FireOnChanged();
  return true;
}

}  // namespace webrtc
