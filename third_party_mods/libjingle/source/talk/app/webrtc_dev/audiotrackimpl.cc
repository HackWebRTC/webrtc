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
#include "talk/app/webrtc_dev/audiotrackimpl.h"

#include <string>

namespace webrtc {

static const char kAudioTrackKind[] = "audio";

AudioTrack::AudioTrack(const std::string& label, uint32 ssrc)
    : enabled_(true),
      label_(label),
      ssrc_(ssrc),
      state_(kInitializing),
      audio_device_(NULL) {
}

AudioTrack::AudioTrack(const std::string& label,
                       AudioDeviceModule* audio_device)
    : enabled_(true),
      label_(label),
      ssrc_(0),
      state_(kInitializing),
      audio_device_(audio_device) {
}

  // Get the AudioDeviceModule associated with this track.
AudioDeviceModule* AudioTrack::GetAudioDevice() {
  return audio_device_.get();
}

  // Implement MediaStreamTrack
const char* AudioTrack::kind() const {
  return kAudioTrackKind;
}

bool AudioTrack::set_enabled(bool enable) {
  bool fire_on_change = (enable != enabled_);
  enabled_ = enable;
  if (fire_on_change)
    NotifierImpl<LocalAudioTrackInterface>::FireOnChanged();
}

bool AudioTrack::set_ssrc(uint32 ssrc) {
  ASSERT(ssrc_ == 0);
  ASSERT(ssrc != 0);
  if (ssrc_ != 0)
    return false;
  ssrc_ = ssrc;
  NotifierImpl<LocalAudioTrackInterface>::FireOnChanged();
  return true;
}

bool AudioTrack::set_state(TrackState new_state) {
  bool fire_on_change = (state_ != new_state);
  state_ = new_state;
  if (fire_on_change)
    NotifierImpl<LocalAudioTrackInterface>::FireOnChanged();
  return true;
}

scoped_refptr<AudioTrackInterface> AudioTrack::Create(
    const std::string& label, uint32 ssrc) {
  talk_base::RefCountImpl<AudioTrack>* track =
      new talk_base::RefCountImpl<AudioTrack>(label, ssrc);
  return track;
}

scoped_refptr<LocalAudioTrackInterface> CreateLocalAudioTrack(
    const std::string& label,
    AudioDeviceModule* audio_device) {
  talk_base::RefCountImpl<AudioTrack>* track =
      new talk_base::RefCountImpl<AudioTrack>(label, audio_device);
  return track;
}

}  // namespace webrtc
