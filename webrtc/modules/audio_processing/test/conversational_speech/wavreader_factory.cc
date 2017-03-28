/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/test/conversational_speech/wavreader_factory.h"

#include "webrtc/modules/audio_processing/test/conversational_speech/wavreader_adaptor.h"

namespace webrtc {
namespace test {
namespace conversational_speech {

WavReaderFactory::WavReaderFactory() = default;

WavReaderFactory::~WavReaderFactory() = default;

std::unique_ptr<WavReaderInterface> WavReaderFactory::Create(
    const std::string& filepath) const {
  return std::unique_ptr<WavReaderAdaptor>(new WavReaderAdaptor(filepath));
}

}  // namespace conversational_speech
}  // namespace test
}  // namespace webrtc
