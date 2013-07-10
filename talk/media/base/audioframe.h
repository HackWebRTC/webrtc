/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#ifndef TALK_MEDIA_BASE_AUDIOFRAME_H_
#define TALK_MEDIA_BASE_AUDIOFRAME_H_

namespace cricket {

class AudioFrame {
 public:
  AudioFrame()
      : audio10ms_(NULL),
        length_(0),
        sampling_frequency_(8000),
        stereo_(false) {
  }
  AudioFrame(int16* audio, size_t audio_length, int sample_freq, bool stereo)
      : audio10ms_(audio),
        length_(audio_length),
        sampling_frequency_(sample_freq),
        stereo_(stereo) {
  }

  int16* GetData() { return audio10ms_; }
  size_t GetSize() const { return length_; }
  int GetSamplingFrequency() const { return sampling_frequency_; }
  bool GetStereo() const { return stereo_; }

 private:
  // TODO(janahan): currently the data is not owned by this class.
  // add ownership when we come up with the first use case that requires it.
  int16* audio10ms_;
  size_t length_;
  int sampling_frequency_;
  bool stereo_;
};

}  // namespace cricket
#endif  // TALK_MEDIA_BASE_AUDIOFRAME_H_
