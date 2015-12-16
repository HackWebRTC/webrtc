/*
 * libjingle
 * Copyright 2015 Google Inc.
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

#include "talk/app/webrtc/mediastreamobserver.h"

#include <algorithm>

namespace webrtc {

MediaStreamObserver::MediaStreamObserver(MediaStreamInterface* stream)
    : stream_(stream),
      cached_audio_tracks_(stream->GetAudioTracks()),
      cached_video_tracks_(stream->GetVideoTracks()) {
  stream_->RegisterObserver(this);
}

MediaStreamObserver::~MediaStreamObserver() {
  stream_->UnregisterObserver(this);
}

void MediaStreamObserver::OnChanged() {
  AudioTrackVector new_audio_tracks = stream_->GetAudioTracks();
  VideoTrackVector new_video_tracks = stream_->GetVideoTracks();

  // Find removed audio tracks.
  for (const auto& cached_track : cached_audio_tracks_) {
    auto it = std::find_if(
        new_audio_tracks.begin(), new_audio_tracks.end(),
        [cached_track](const AudioTrackVector::value_type& new_track) {
          return new_track->id().compare(cached_track->id()) == 0;
        });
    if (it == new_audio_tracks.end()) {
      SignalAudioTrackRemoved(cached_track.get(), stream_);
    }
  }

  // Find added audio tracks.
  for (const auto& new_track : new_audio_tracks) {
    auto it = std::find_if(
        cached_audio_tracks_.begin(), cached_audio_tracks_.end(),
        [new_track](const AudioTrackVector::value_type& cached_track) {
          return new_track->id().compare(cached_track->id()) == 0;
        });
    if (it == cached_audio_tracks_.end()) {
      SignalAudioTrackAdded(new_track.get(), stream_);
    }
  }

  // Find removed video tracks.
  for (const auto& cached_track : cached_video_tracks_) {
    auto it = std::find_if(
        new_video_tracks.begin(), new_video_tracks.end(),
        [cached_track](const VideoTrackVector::value_type& new_track) {
          return new_track->id().compare(cached_track->id()) == 0;
        });
    if (it == new_video_tracks.end()) {
      SignalVideoTrackRemoved(cached_track.get(), stream_);
    }
  }

  // Find added video tracks.
  for (const auto& new_track : new_video_tracks) {
    auto it = std::find_if(
        cached_video_tracks_.begin(), cached_video_tracks_.end(),
        [new_track](const VideoTrackVector::value_type& cached_track) {
          return new_track->id().compare(cached_track->id()) == 0;
        });
    if (it == cached_video_tracks_.end()) {
      SignalVideoTrackAdded(new_track.get(), stream_);
    }
  }

  cached_audio_tracks_ = new_audio_tracks;
  cached_video_tracks_ = new_video_tracks;
}

}  // namespace webrtc
