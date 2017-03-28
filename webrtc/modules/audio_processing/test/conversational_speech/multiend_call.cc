/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/test/conversational_speech/multiend_call.h"

#include <utility>

#include "webrtc/base/pathutils.h"

namespace webrtc {
namespace test {
namespace conversational_speech {

MultiEndCall::MultiEndCall(
    rtc::ArrayView<const Turn> timing, const std::string& audiotracks_path,
    std::unique_ptr<WavReaderAbstractFactory> wavreader_abstract_factory)
        : timing_(timing), audiotracks_path_(audiotracks_path),
          wavreader_abstract_factory_(std::move(wavreader_abstract_factory)) {
  FindSpeakerNames();
  CreateAudioTrackReaders();
  CheckTiming();
}

MultiEndCall::~MultiEndCall() = default;

const std::set<std::string>& MultiEndCall::speaker_names() const {
  return speaker_names_;
}

const std::map<std::string, std::unique_ptr<WavReaderInterface>>&
    MultiEndCall::audiotrack_readers() const {
  return audiotrack_readers_;
}

void MultiEndCall::FindSpeakerNames() {
  RTC_DCHECK(speaker_names_.empty());
  for (const Turn& turn : timing_) {
    speaker_names_.insert(turn.speaker_name);
  }
}

void MultiEndCall::CreateAudioTrackReaders() {
  RTC_DCHECK(audiotrack_readers_.empty());
  for (const Turn& turn : timing_) {
    auto it = audiotrack_readers_.find(turn.audiotrack_file_name);
    if (it != audiotrack_readers_.end())
      continue;

    // Instance Pathname to retrieve the full path to the audiotrack file.
    const rtc::Pathname audiotrack_file_path(
        audiotracks_path_, turn.audiotrack_file_name);

    // Map the audiotrack file name to a new instance of WavReaderInterface.
    std::unique_ptr<WavReaderInterface> wavreader =
        wavreader_abstract_factory_->Create(audiotrack_file_path.pathname());
    audiotrack_readers_.insert(std::make_pair(
        turn.audiotrack_file_name, std::move(wavreader)));
  }
}

void MultiEndCall::CheckTiming() {
  // TODO(alessiob): use audiotrack lengths and offset to check whether the
  // timing is valid.
}

}  // namespace conversational_speech
}  // namespace test
}  // namespace webrtc
