/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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
#include "talk/app/webrtc/local_stream_dev.h"

namespace webrtc {

class LocalAudioTrackImpl : public NotifierImpl<LocalAudioTrack> {
 public:
  LocalAudioTrackImpl(){};
  LocalAudioTrackImpl(AudioDevice* audio_device)
      : enabled_(true),
        kind_(kAudioTrackKind),
        audio_device_(audio_device) {
  }

  // Get the AudioDevice associated with this track.
  virtual scoped_refptr<AudioDevice> GetAudioDevice() {
    return audio_device_.get();
  };

  // Implement MediaStreamTrack
  virtual const std::string& kind() {
    return kind_;
  }

  virtual const std::string& label() {
    return audio_device_->name();
  }

  virtual bool enabled() {
    return enabled_;
  }

  virtual bool set_enabled(bool enable) {
    bool fire_on_change = enable != enabled_;
    enabled_ = enable;
    if (fire_on_change)
      NotifierImpl<LocalAudioTrack>::FireOnChanged();
  }

 private:
  bool enabled_;
  std::string kind_;
  scoped_refptr<AudioDevice> audio_device_;
};

scoped_refptr<LocalAudioTrack> LocalAudioTrack::Create(AudioDevice* audio_device) {
  RefCountImpl<LocalAudioTrackImpl>* lstream =
      new RefCountImpl<LocalAudioTrackImpl>(audio_device);
  return lstream;
}

} // namespace webrtc
