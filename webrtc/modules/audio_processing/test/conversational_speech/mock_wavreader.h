/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_TEST_CONVERSATIONAL_SPEECH_MOCK_WAVREADER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_TEST_CONVERSATIONAL_SPEECH_MOCK_WAVREADER_H_

#include <cstddef>
#include <string>

#include "webrtc/modules/audio_processing/test/conversational_speech/wavreader_interface.h"
#include "webrtc/test/gmock.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace test {
namespace conversational_speech {

class MockWavReader : public WavReaderInterface {
 public:
  MockWavReader(int sample_rate, size_t num_channels, size_t num_samples);
  ~MockWavReader();

  // TODO(alessiob): use ON_CALL to return random samples.
  MOCK_METHOD2(ReadFloatSamples, size_t(size_t, float*));
  MOCK_METHOD2(ReadInt16Samples, size_t(size_t, int16_t*));

  MOCK_CONST_METHOD0(sample_rate, int());
  MOCK_CONST_METHOD0(num_channels, size_t());
  MOCK_CONST_METHOD0(num_samples, size_t());

 private:
  const int sample_rate_;
  const size_t num_channels_;
  const size_t num_samples_;
};

}  // namespace conversational_speech
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_TEST_CONVERSATIONAL_SPEECH_MOCK_WAVREADER_H_
