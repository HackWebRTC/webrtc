/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <array>
#include <string>
#include <vector>

#include "common_audio/resampler/push_sinc_resampler.h"
#include "common_audio/wav_file.h"
#include "modules/audio_processing/agc2/rnn_vad/common.h"
#include "rtc_base/flags.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace test {
namespace {

using rnn_vad::kFrameSize10ms24kHz;

DEFINE_string(i, "", "Path to the input wav file");
DEFINE_string(f, "", "Path to the output features file");
DEFINE_string(o, "", "Path to the output VAD probabilities file");
DEFINE_bool(help, false, "Prints this message");

}  // namespace

int main(int argc, char* argv[]) {
  rtc::LogMessage::LogToDebug(rtc::LS_INFO);
  rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true);
  if (FLAG_help) {
    rtc::FlagList::Print(nullptr, false);
    return 0;
  }

  // Open wav input file and check properties.
  WavReader wav_reader(FLAG_i);
  if (wav_reader.num_channels() != 1) {
    RTC_LOG(LS_ERROR) << "Only mono wav files are supported";
    return 1;
  }
  if (wav_reader.sample_rate() % 100 != 0) {
    RTC_LOG(LS_ERROR) << "The sample rate rate must allow 10 ms frames.";
    return 1;
  }

  // Init output files.
  FILE* vad_probs_file = fopen(FLAG_o, "wb");
  FILE* features_file = nullptr;
  if (std::string::empty(FLAG_f)) {
    features_file = fopen(FLAG_f, "wb");
  }

  // Init resampling.
  const size_t frame_size_10ms =
      rtc::CheckedDivExact(wav_reader.sample_rate(), 100);
  std::vector<float> samples_10ms;
  samples_10ms.resize(frame_size_10ms);
  std::array<float, kFrameSize10ms24kHz> samples_10ms_24kHz;
  PushSincResampler resampler(frame_size_10ms, kFrameSize10ms24kHz);

  // TODO(alessiob): Init feature extractor and RNN-based VAD.

  // Compute VAD probabilities.
  while (true) {
    // Read frame at the input sample rate.
    const auto read_samples =
        wav_reader.ReadSamples(frame_size_10ms, samples_10ms.data());
    if (read_samples < frame_size_10ms) {
      break;  // EOF.
    }
    // Resample input.
    resampler.Resample(samples_10ms.data(), samples_10ms.size(),
                       samples_10ms_24kHz.data(), samples_10ms_24kHz.size());

    // TODO(alessiob): Extract features.
    float vad_probability;
    bool is_silence = true;

    // Write features.
    if (features_file) {
      const float float_is_silence = is_silence ? 1.f : 0.f;
      fwrite(&float_is_silence, sizeof(float), 1, features_file);
      // TODO(alessiob): Write feature vector.
    }

    // Compute VAD probability.
    if (is_silence) {
      vad_probability = 0.f;
      // TODO(alessiob): Reset VAD.
    } else {
      // TODO(alessiob): Compute VAD probability.
    }
    RTC_DCHECK_GE(vad_probability, 0.f);
    RTC_DCHECK_GE(1.f, vad_probability);
    fwrite(&vad_probability, sizeof(float), 1, vad_probs_file);
  }
  // Close output file(s).
  fclose(vad_probs_file);
  RTC_LOG(LS_INFO) << "VAD probabilities written to " << FLAG_o;
  if (features_file) {
    fclose(features_file);
    RTC_LOG(LS_INFO) << "features written to " << FLAG_f;
  }

  return 0;
}

}  // namespace test
}  // namespace webrtc

int main(int argc, char* argv[]) {
  return webrtc::test::main(argc, argv);
}
