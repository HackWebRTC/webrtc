/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_TEST_CONVERSATIONAL_SPEECH_MULTIEND_CALL_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_TEST_CONVERSATIONAL_SPEECH_MULTIEND_CALL_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "webrtc/base/array_view.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_processing/test/conversational_speech/timing.h"
#include "webrtc/modules/audio_processing/test/conversational_speech/wavreader_abstract_factory.h"
#include "webrtc/modules/audio_processing/test/conversational_speech/wavreader_interface.h"

namespace webrtc {
namespace test {
namespace conversational_speech {

class MultiEndCall {
 public:
  MultiEndCall(
      rtc::ArrayView<const Turn> timing, const std::string& audiotracks_path,
      std::unique_ptr<WavReaderAbstractFactory> wavreader_abstract_factory);
  ~MultiEndCall();

  const std::set<std::string>& speaker_names() const;
  const std::map<std::string, std::unique_ptr<WavReaderInterface>>&
      audiotrack_readers() const;

 private:
  // Find unique speaker names.
  void FindSpeakerNames();

  // Create one WavReader instance for each unique audiotrack.
  void CreateAudioTrackReaders();

  // Check the speaking turns timing.
  void CheckTiming();

  rtc::ArrayView<const Turn> timing_;
  const std::string& audiotracks_path_;
  std::unique_ptr<WavReaderAbstractFactory> wavreader_abstract_factory_;
  std::set<std::string> speaker_names_;
  std::map<std::string, std::unique_ptr<WavReaderInterface>>
      audiotrack_readers_;

  RTC_DISALLOW_COPY_AND_ASSIGN(MultiEndCall);
};

}  // namespace conversational_speech
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_TEST_CONVERSATIONAL_SPEECH_MULTIEND_CALL_H_
