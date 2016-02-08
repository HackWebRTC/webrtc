/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_BASE_AUDIOFRAME_H_
#define WEBRTC_MEDIA_BASE_AUDIOFRAME_H_

namespace cricket {

class AudioFrame {
 public:
  AudioFrame()
      : audio10ms_(NULL),
        length_(0),
        sampling_frequency_(8000),
        stereo_(false) {
  }
  AudioFrame(int16_t* audio, size_t audio_length, int sample_freq, bool stereo)
      : audio10ms_(audio),
        length_(audio_length),
        sampling_frequency_(sample_freq),
        stereo_(stereo) {}

  int16_t* GetData() { return audio10ms_; }
  size_t GetSize() const { return length_; }
  int GetSamplingFrequency() const { return sampling_frequency_; }
  bool GetStereo() const { return stereo_; }

 private:
  // TODO(janahan): currently the data is not owned by this class.
  // add ownership when we come up with the first use case that requires it.
  int16_t* audio10ms_;
  size_t length_;
  int sampling_frequency_;
  bool stereo_;
};

}  // namespace cricket
#endif  // WEBRTC_MEDIA_BASE_AUDIOFRAME_H_
