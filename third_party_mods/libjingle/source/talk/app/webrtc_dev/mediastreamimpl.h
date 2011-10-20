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

// This file contains the implementation of MediaStreamInterface interface.

#ifndef TALK_APP_WEBRTC_MEDIASTREAMIMPL_H_
#define TALK_APP_WEBRTC_MEDIASTREAMIMPL_H_

#include <string>
#include <vector>

#include "talk/app/webrtc_dev/mediastream.h"
#include "talk/app/webrtc_dev/notifierimpl.h"

namespace webrtc {
class AudioTrack;
class VideoTrack;

class MediaStream : public Notifier<LocalMediaStreamInterface> {
 public:
  template<class T>
  class MediaStreamTrackList : public MediaStreamTrackListInterface<T> {
   public:
    void AddTrack(T* track) {
      tracks_.push_back(track);
    }
    virtual size_t count() { return tracks_.size(); }
    virtual T* at(size_t index) {
      return tracks_.at(index);
    }

   private:
    std::vector<talk_base::scoped_refptr<T> > tracks_;
  };

  static talk_base::scoped_refptr<MediaStream> Create(const std::string& label);

  // Implement LocalMediaStreamInterface.
  virtual bool AddTrack(AudioTrackInterface* track);
  virtual bool AddTrack(VideoTrackInterface* track);
  // Implement MediaStreamInterface.
  virtual std::string label() const { return label_; }
  virtual MediaStreamTrackListInterface<AudioTrackInterface>* audio_tracks() {
    return audio_track_list_;
  }
  virtual MediaStreamTrackListInterface<VideoTrackInterface>* video_tracks() {
    return video_track_list_;
  }
  virtual ReadyState ready_state() { return ready_state_; }
  virtual void set_ready_state(ReadyState new_state);
  void set_state(ReadyState new_state);

 protected:
  explicit MediaStream(const std::string& label);

  std::string label_;
  MediaStreamInterface::ReadyState ready_state_;
  talk_base::scoped_refptr<MediaStreamTrackList<AudioTrackInterface> >
      audio_track_list_;
  talk_base::scoped_refptr<MediaStreamTrackList<VideoTrackInterface> >
      video_track_list_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_MEDIASTREAMIMPL_H_
