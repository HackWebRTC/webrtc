/*
 * libjingle
 * Copyright 2013 Google Inc.
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

#ifndef TALK_APP_WEBRTC_AUDIOTRACKRENDERER_H_
#define TALK_APP_WEBRTC_AUDIOTRACKRENDERER_H_

#include "talk/media/base/audiorenderer.h"
#include "webrtc/base/thread.h"

namespace webrtc {

// Class used for AudioTrack to get the ID of WebRtc voice channel that
// the AudioTrack is connecting to.
// Each AudioTrack owns a AudioTrackRenderer instance.
// AddChannel() will be called when an AudioTrack is added to a MediaStream.
// RemoveChannel will be called when the AudioTrack or WebRtc VoE channel is
// going away.
// This implementation only supports one channel, and it is only used by
// Chrome for remote audio tracks."
class AudioTrackRenderer : public cricket::AudioRenderer {
 public:
  AudioTrackRenderer();
  ~AudioTrackRenderer();

  // Implements cricket::AudioRenderer.
  void AddChannel(int channel_id) override;
  void RemoveChannel(int channel_id) override;

 private:
  int channel_id_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_AUDIOTRACKRENDERER_H_
