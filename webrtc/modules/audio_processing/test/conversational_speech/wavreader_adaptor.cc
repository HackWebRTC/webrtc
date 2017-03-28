/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/test/conversational_speech/wavreader_adaptor.h"

#include "webrtc/base/checks.h"

namespace webrtc {
namespace test {
namespace conversational_speech {

WavReaderAdaptor::WavReaderAdaptor(const std::string& filepath) {
  // TODO(alessiob): implement.
}

WavReaderAdaptor::~WavReaderAdaptor() {}

size_t WavReaderAdaptor::ReadFloatSamples(size_t num_samples, float* samples) {
  // TODO(alessiob): implement.
  FATAL();
  return 0u;
}

size_t WavReaderAdaptor::ReadInt16Samples(
    size_t num_samples, int16_t* samples) {
  // TODO(alessiob): implement.
  FATAL();
  return 0u;
}

int WavReaderAdaptor::sample_rate() const {
  // TODO(alessiob): implement.
  FATAL();
  return 0;
}

size_t WavReaderAdaptor::num_channels() const {
  // TODO(alessiob): implement.
  FATAL();
  return 0u;
}

size_t WavReaderAdaptor::num_samples() const {
  // TODO(alessiob): implement.
  FATAL();
  return 0u;
}

}  // namespace conversational_speech
}  // namespace test
}  // namespace webrtc
