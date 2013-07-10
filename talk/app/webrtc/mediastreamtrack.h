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

#ifndef TALK_APP_WEBRTC_MEDIASTREAMTRACK_H_
#define TALK_APP_WEBRTC_MEDIASTREAMTRACK_H_

#include <string>

#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/app/webrtc/notifier.h"

namespace webrtc {

// MediaTrack implements the interface common to AudioTrackInterface and
// VideoTrackInterface.
template <typename T>
class MediaStreamTrack : public Notifier<T> {
 public:
  typedef typename T::TrackState TypedTrackState;

  virtual std::string id() const { return id_; }
  virtual MediaStreamTrackInterface::TrackState state() const {
    return state_;
  }
  virtual bool enabled() const { return enabled_; }
  virtual bool set_enabled(bool enable) {
    bool fire_on_change = (enable != enabled_);
    enabled_ = enable;
    if (fire_on_change) {
      Notifier<T>::FireOnChanged();
    }
    return fire_on_change;
  }
  virtual bool set_state(MediaStreamTrackInterface::TrackState new_state) {
    bool fire_on_change = (state_ != new_state);
    state_ = new_state;
    if (fire_on_change)
      Notifier<T>::FireOnChanged();
    return true;
  }

 protected:
  explicit MediaStreamTrack(const std::string& id)
      : enabled_(true),
        id_(id),
        state_(MediaStreamTrackInterface::kInitializing) {
  }

 private:
  bool enabled_;
  std::string id_;
  MediaStreamTrackInterface::TrackState state_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_MEDIASTREAMTRACK_H_
