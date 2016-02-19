/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
//  Command line tool for speech intelligibility enhancement. Provides for
//  running and testing intelligibility_enhancer as an independent process.
//  Use --help for options.
//

#include <sys/stat.h>

#include "gflags/gflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/common_audio/include/audio_util.h"
#include "webrtc/common_audio/wav_file.h"
#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/modules/audio_processing/intelligibility/intelligibility_enhancer.h"
#include "webrtc/modules/audio_processing/noise_suppression_impl.h"

using std::complex;

namespace webrtc {
namespace {

DEFINE_double(clear_alpha, 0.9, "Power decay factor for clear data.");
DEFINE_int32(sample_rate,
             16000,
             "Audio sample rate used in the input and output files.");
DEFINE_int32(ana_rate,
             60,
             "Analysis rate; gains recalculated every N blocks.");
DEFINE_double(gain_limit, 1000.0, "Maximum gain change in one block.");

DEFINE_string(clear_file, "speech.wav", "Input file with clear speech.");
DEFINE_string(noise_file, "noise.wav", "Input file with noise data.");
DEFINE_string(out_file, "proc_enhanced.wav", "Enhanced output file.");

const size_t kNumChannels = 1;

// void function for gtest
void void_main(int argc, char* argv[]) {
  google::SetUsageMessage(
      "\n\nInput files must be little-endian 16-bit signed raw PCM.\n");
  google::ParseCommandLineFlags(&argc, &argv, true);

  size_t samples;        // Number of samples in input PCM file
  size_t fragment_size;  // Number of samples to process at a time
                         // to simulate APM stream processing

  // Load settings and wav input.

  fragment_size = FLAGS_sample_rate / 100;  // Mirror real time APM chunk size.
                                            // Duplicates chunk_length_ in
                                            // IntelligibilityEnhancer.

  struct stat in_stat, noise_stat;
  ASSERT_EQ(stat(FLAGS_clear_file.c_str(), &in_stat), 0)
      << "Empty speech file.";
  ASSERT_EQ(stat(FLAGS_noise_file.c_str(), &noise_stat), 0)
      << "Empty noise file.";

  samples = std::min(in_stat.st_size, noise_stat.st_size) / 2;

  WavReader in_file(FLAGS_clear_file);
  std::vector<float> in_fpcm(samples);
  in_file.ReadSamples(samples, &in_fpcm[0]);
  FloatS16ToFloat(&in_fpcm[0], samples, &in_fpcm[0]);

  WavReader noise_file(FLAGS_noise_file);
  std::vector<float> noise_fpcm(samples);
  noise_file.ReadSamples(samples, &noise_fpcm[0]);
  FloatS16ToFloat(&noise_fpcm[0], samples, &noise_fpcm[0]);

  // Run intelligibility enhancement.
  IntelligibilityEnhancer::Config config;
  config.sample_rate_hz = FLAGS_sample_rate;
  config.decay_rate = static_cast<float>(FLAGS_clear_alpha);
  config.analysis_rate = FLAGS_ana_rate;
  config.gain_change_limit = FLAGS_gain_limit;
  IntelligibilityEnhancer enh(config);
  rtc::CriticalSection crit;
  NoiseSuppressionImpl ns(&crit);
  ns.Initialize(kNumChannels, FLAGS_sample_rate);
  ns.Enable(true);

  AudioBuffer capture_audio(fragment_size,
                            kNumChannels,
                            fragment_size,
                            kNumChannels,
                            fragment_size);
  StreamConfig stream_config(FLAGS_sample_rate, kNumChannels);

  // Slice the input into smaller chunks, as the APM would do, and feed them
  // through the enhancer.
  float* clear_cursor = &in_fpcm[0];
  float* noise_cursor = &noise_fpcm[0];

  for (size_t i = 0; i < samples; i += fragment_size) {
    capture_audio.CopyFrom(&noise_cursor, stream_config);
    ns.AnalyzeCaptureAudio(&capture_audio);
    ns.ProcessCaptureAudio(&capture_audio);
    enh.SetCaptureNoiseEstimate(ns.NoiseEstimate());
    enh.ProcessRenderAudio(&clear_cursor, FLAGS_sample_rate, kNumChannels);
    clear_cursor += fragment_size;
    noise_cursor += fragment_size;
  }

  FloatToFloatS16(&in_fpcm[0], samples, &in_fpcm[0]);

  WavWriter out_file(FLAGS_out_file, FLAGS_sample_rate, kNumChannels);
  out_file.WriteSamples(&in_fpcm[0], samples);
}

}  // namespace
}  // namespace webrtc

int main(int argc, char* argv[]) {
  webrtc::void_main(argc, argv);
  return 0;
}
