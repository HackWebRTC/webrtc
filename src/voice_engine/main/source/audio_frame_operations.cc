/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio_frame_operations.h"
#include "module_common_types.h"

namespace webrtc {
namespace voe {

int AudioFrameOperations::MonoToStereo(AudioFrame& frame) {
  if (frame.num_channels_ != 1) {
    return -1;
  }
  if ((frame.samples_per_channel_ << 1) >=
      AudioFrame::kMaxDataSizeSamples) {
    // not enough memory to expand from mono to stereo
    return -1;
  }

  int16_t payloadCopy[AudioFrame::kMaxDataSizeSamples];
  memcpy(payloadCopy, frame.data_,
         sizeof(int16_t) * frame.samples_per_channel_);

  for (int i = 0; i < frame.samples_per_channel_; i++) {
    frame.data_[2 * i]   = payloadCopy[i];
    frame.data_[2 * i + 1] = payloadCopy[i];
  }

  frame.num_channels_ = 2;

  return 0;
}

int AudioFrameOperations::StereoToMono(AudioFrame& frame) {
  if (frame.num_channels_ != 2) {
    return -1;
  }

  for (int i = 0; i < frame.samples_per_channel_; i++) {
    frame.data_[i] = (frame.data_[2 * i] >> 1) +
                            (frame.data_[2 * i + 1] >> 1);
  }

  frame.num_channels_ = 1;

  return 0;
}

void AudioFrameOperations::SwapStereoChannels(AudioFrame* frame) {
  if (frame->num_channels_ != 2) return;

  for (int i = 0; i < frame->samples_per_channel_ * 2; i += 2) {
    int16_t temp_data = frame->data_[i];
    frame->data_[i] = frame->data_[i + 1];
    frame->data_[i + 1] = temp_data;
  }
}

void AudioFrameOperations::Mute(AudioFrame& frame) {
  memset(frame.data_, 0, sizeof(int16_t) *
      frame.samples_per_channel_ * frame.num_channels_);
  frame.energy_ = 0;
}

int AudioFrameOperations::Scale(float left, float right, AudioFrame& frame) {
  if (frame.num_channels_ != 2) {
    return -1;
  }

  for (int i = 0; i < frame.samples_per_channel_; i++) {
    frame.data_[2 * i] =
        static_cast<int16_t>(left * frame.data_[2 * i]);
    frame.data_[2 * i + 1] =
        static_cast<int16_t>(right * frame.data_[2 * i + 1]);
  }
  return 0;
}

int AudioFrameOperations::ScaleWithSat(float scale, AudioFrame& frame) {
  int32_t temp_data = 0;

  // Ensure that the output result is saturated [-32768, +32767].
  for (int i = 0; i < frame.samples_per_channel_ * frame.num_channels_;
       i++) {
    temp_data = static_cast<int32_t>(scale * frame.data_[i]);
    if (temp_data < -32768) {
      frame.data_[i] = -32768;
    } else if (temp_data > 32767) {
      frame.data_[i] = 32767;
    } else {
      frame.data_[i] = static_cast<int16_t>(temp_data);
    }
  }
  return 0;
}

}  //  namespace voe
}  //  namespace webrtc

