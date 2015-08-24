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

#ifndef TALK_MEDIA_BASE_AUDIORENDERER_H_
#define TALK_MEDIA_BASE_AUDIORENDERER_H_

#include <cstddef>

namespace cricket {

// Abstract interface for rendering the audio data.
class AudioRenderer {
 public:
  class Sink {
   public:
    // Callback to receive data from the AudioRenderer.
    virtual void OnData(const void* audio_data,
                        int bits_per_sample,
                        int sample_rate,
                        int number_of_channels,
                        size_t number_of_frames) = 0;

    // Called when the AudioRenderer is going away.
    virtual void OnClose() = 0;

   protected:
    virtual ~Sink() {}
  };

  // Sets a sink to the AudioRenderer. There can be only one sink connected
  // to the renderer at a time.
  virtual void SetSink(Sink* sink) {}

  // Add the WebRtc VoE channel to the renderer.
  // For local stream, multiple WebRtc VoE channels can be connected to the
  // renderer. While for remote stream, only one WebRtc VoE channel can be
  // connected to the renderer.
  // TODO(xians): Remove this interface after Chrome switches to the
  // AudioRenderer::Sink interface.
  virtual void AddChannel(int channel_id) {}

  // Remove the WebRtc VoE channel from the renderer.
  // This method is called when the VoE channel is going away.
  // TODO(xians): Remove this interface after Chrome switches to the
  // AudioRenderer::Sink interface.
  virtual void RemoveChannel(int channel_id) {}

 protected:
  virtual ~AudioRenderer() {}
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_AUDIORENDERER_H_
