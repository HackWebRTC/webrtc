/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/test/audio_buffer_tools.h"

namespace webrtc {
namespace test {

void SetupFrame(StreamConfig stream_config,
                std::vector<float*>* frame,
                std::vector<float>* frame_samples) {
  frame_samples->resize(stream_config.num_channels() *
                        stream_config.num_frames());
  frame->resize(stream_config.num_channels());
  for (size_t ch = 0; ch < stream_config.num_channels(); ++ch) {
    (*frame)[ch] = &(*frame_samples)[ch * stream_config.num_frames()];
  }
}

void CopyVectorToAudioBuffer(const StreamConfig& stream_config,
                             const std::vector<float>& source,
                             AudioBuffer* destination) {
  std::vector<float*> input;
  std::vector<float> input_samples;

  SetupFrame(stream_config, &input, &input_samples);

  RTC_DCHECK_EQ(input_samples.size(), source.size());
  input_samples = source;

  destination->CopyFrom(&input[0], stream_config);
}

std::vector<float> ExtractVectorFromAudioBuffer(
    const StreamConfig& stream_config,
    AudioBuffer* source) {
  std::vector<float*> output;
  std::vector<float> output_samples;

  SetupFrame(stream_config, &output, &output_samples);

  source->CopyTo(stream_config, &output[0]);

  return output_samples;
}

}  // namespace test
}  // namespace webrtc
