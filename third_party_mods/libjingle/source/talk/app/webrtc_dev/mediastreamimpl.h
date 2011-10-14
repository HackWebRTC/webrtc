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

#ifndef TALK_APP_WEBRTC_MEDIASTREAMIMPL_H_
#define TALK_APP_WEBRTC_MEDIASTREAMIMPL_H_

#include <string>
#include <vector>

#include "talk/app/webrtc_dev/mediastream.h"
#include "talk/app/webrtc_dev/notifierimpl.h"

namespace webrtc {

class MediaStreamImpl
    : public NotifierImpl<LocalMediaStreamInterface> {
 public:
  class MediaStreamTrackListImpl :
    public NotifierImpl<MediaStreamTrackListInterface> {
   public:
    void AddTrack(MediaStreamTrackInterface* track);
    virtual size_t count() { return tracks_.size(); }
    virtual MediaStreamTrackInterface* at(size_t index) {
      return tracks_.at(index);
    }

   private:
    std::vector<scoped_refptr<MediaStreamTrackInterface> > tracks_;
  };

  static scoped_refptr<MediaStreamImpl> Create(const std::string& label);

  // Implement LocalStream.
  virtual bool AddTrack(MediaStreamTrackInterface* track);

  // Implement MediaStream.
  virtual const std::string& label() { return label_; }
  virtual MediaStreamTrackListInterface* tracks() { return track_list_; }
  virtual ReadyState ready_state() { return ready_state_; }
  virtual void set_ready_state(ReadyState new_state);
  void set_state(ReadyState new_state);

 protected:
  explicit MediaStreamImpl(const std::string& label);

  std::string label_;
  MediaStreamInterface::ReadyState ready_state_;
  scoped_refptr<MediaStreamTrackListImpl> track_list_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_MEDIASTREAMIMPL_H_
