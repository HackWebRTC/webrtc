/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_TEST_CONVERSATIONAL_SPEECH_WAVREADER_ADAPTOR_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_TEST_CONVERSATIONAL_SPEECH_WAVREADER_ADAPTOR_H_

#include "webrtc/modules/audio_processing/test/conversational_speech/wavreader_interface.h"

#include <cstddef>
#include <string>

#include "webrtc/typedefs.h"

namespace webrtc {
namespace test {
namespace conversational_speech {

class WavReaderAdaptor : public WavReaderInterface {
 public:
  explicit WavReaderAdaptor(const std::string& filepath);
  ~WavReaderAdaptor() override;

  size_t ReadFloatSamples(size_t num_samples, float* samples) override;
  size_t ReadInt16Samples(size_t num_samples, int16_t* samples) override;

  int sample_rate() const override;
  size_t num_channels() const override;
  size_t num_samples() const override;
};

}  // namespace conversational_speech
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_TEST_CONVERSATIONAL_SPEECH_WAVREADER_ADAPTOR_H_
