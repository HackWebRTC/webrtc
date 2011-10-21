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
    : MediaTrack<LocalAudioTrackInterface>(label, ssrc),
      audio_device_(NULL) {
}

AudioTrack::AudioTrack(const std::string& label,
                       AudioDeviceModule* audio_device)
    : MediaTrack<LocalAudioTrackInterface>(label, 0),
      audio_device_(audio_device) {
}

  // Get the AudioDeviceModule associated with this track.
AudioDeviceModule* AudioTrack::GetAudioDevice() {
  return audio_device_.get();
}

  // Implement MediaStreamTrack
std::string AudioTrack::kind() const {
  return std::string(kAudioTrackKind);
}

talk_base::scoped_refptr<AudioTrack> AudioTrack::CreateRemote(
    const std::string& label,
    uint32 ssrc) {
  talk_base::RefCount<AudioTrack>* track =
      new talk_base::RefCount<AudioTrack>(label, ssrc);
  return track;
}

talk_base::scoped_refptr<AudioTrack> AudioTrack::CreateLocal(
    const std::string& label,
    AudioDeviceModule* audio_device) {
  talk_base::RefCount<AudioTrack>* track =
      new talk_base::RefCount<AudioTrack>(label, audio_device);
  return track;
}

}  // namespace webrtc
