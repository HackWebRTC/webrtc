/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/test/conversational_speech/settings.h"

namespace webrtc {
namespace test {

const std::string& ConvSpeechGeneratorSettings::audiotracks_path() const {
  return audiotracks_path_;
}

const std::string& ConvSpeechGeneratorSettings::timing_filepath() const {
  return timing_filepath_;
}

const std::string& ConvSpeechGeneratorSettings::output_path() const {
  return output_path_;
}

}  // namespace test
}  // namespace webrtc
