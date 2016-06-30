/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/array_view.h"
#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/level_controller/level_controller.h"
#include "webrtc/modules/audio_processing/test/audio_buffer_tools.h"
#include "webrtc/modules/audio_processing/test/bitexactness_tools.h"

namespace webrtc {
namespace {

const int kNumFramesToProcess = 1000;

// Processes a specified amount of frames, verifies the results and reports
// any errors.
void RunBitexactnessTest(int sample_rate_hz,
                         size_t num_channels,
                         rtc::ArrayView<const float> output_reference) {
  LevelController level_controller;
  level_controller.Initialize(sample_rate_hz);

  int samples_per_channel = rtc::CheckedDivExact(sample_rate_hz, 100);
  const StreamConfig capture_config(sample_rate_hz, num_channels, false);
  AudioBuffer capture_buffer(
      capture_config.num_frames(), capture_config.num_channels(),
      capture_config.num_frames(), capture_config.num_channels(),
      capture_config.num_frames());
  test::InputAudioFile capture_file(
      test::GetApmCaptureTestVectorFileName(sample_rate_hz));
  std::vector<float> capture_input(samples_per_channel * num_channels);
  for (size_t frame_no = 0; frame_no < kNumFramesToProcess; ++frame_no) {
    ReadFloatSamplesFromStereoFile(samples_per_channel, num_channels,
                                   &capture_file, capture_input);

    test::CopyVectorToAudioBuffer(capture_config, capture_input,
                                  &capture_buffer);

    level_controller.Process(&capture_buffer);
  }

  // Extract test results.
  std::vector<float> capture_output;
  test::ExtractVectorFromAudioBuffer(capture_config, &capture_buffer,
                                     &capture_output);

  // Compare the output with the reference. Only the first values of the output
  // from last frame processed are compared in order not having to specify all
  // preceding frames as testvectors. As the algorithm being tested has a
  // memory, testing only the last frame implicitly also tests the preceeding
  // frames.
  const float kVectorElementErrorBound = 1.0f / 32768.0f;
  EXPECT_TRUE(test::VerifyDeinterleavedArray(
      capture_config.num_frames(), capture_config.num_channels(),
      output_reference, capture_output, kVectorElementErrorBound));
}

}  // namespace

TEST(LevelControlBitExactnessTest, DISABLED_Mono8kHz) {
  const float kOutputReference[] = {-0.023242f, -0.020266f, -0.015097f};
  RunBitexactnessTest(AudioProcessing::kSampleRate8kHz, 1, kOutputReference);
}

TEST(LevelControlBitExactnessTest, DISABLED_Mono16kHz) {
  const float kOutputReference[] = {-0.019461f, -0.018761f, -0.018481f};
  RunBitexactnessTest(AudioProcessing::kSampleRate16kHz, 1, kOutputReference);
}

TEST(LevelControlBitExactnessTest, DISABLED_Mono32kHz) {
  const float kOutputReference[] = {-0.016872f, -0.019118f, -0.018722f};
  RunBitexactnessTest(AudioProcessing::kSampleRate32kHz, 1, kOutputReference);
}

// TODO(peah): Investigate why this particular testcase differ between Android
// and the rest of the platforms.
TEST(LevelControlBitExactnessTest, DISABLED_Mono48kHz) {
#if !(defined(WEBRTC_ARCH_ARM64) || defined(WEBRTC_ARCH_ARM) || \
      defined(WEBRTC_ANDROID))
  const float kOutputReference[] = {-0.016771f, -0.017831f, -0.020482f};
#else
  const float kOutputReference[] = {-0.015949f, -0.016957f, -0.019478f};
#endif
  RunBitexactnessTest(AudioProcessing::kSampleRate48kHz, 1, kOutputReference);
}

TEST(LevelControlBitExactnessTest, DISABLED_Stereo8kHz) {
  const float kOutputReference[] = {-0.019304f, -0.011600f, -0.016690f,
                                    -0.071335f, -0.031849f, -0.065694f};
  RunBitexactnessTest(AudioProcessing::kSampleRate8kHz, 2, kOutputReference);
}

TEST(LevelControlBitExactnessTest, DISABLED_Stereo16kHz) {
  const float kOutputReference[] = {-0.016302f, -0.007559f, -0.015668f,
                                    -0.068346f, -0.031476f, -0.066065f};
  RunBitexactnessTest(AudioProcessing::kSampleRate16kHz, 2, kOutputReference);
}

TEST(LevelControlBitExactnessTest, DISABLED_Stereo32kHz) {
  const float kOutputReference[] = {-0.013944f, -0.008337f, -0.015972f,
                                    -0.063563f, -0.031233f, -0.066784f};
  RunBitexactnessTest(AudioProcessing::kSampleRate32kHz, 2, kOutputReference);
}

TEST(LevelControlBitExactnessTest, DISABLED_Stereo48kHz) {
  const float kOutputReference[] = {-0.013652f, -0.008125f, -0.014593f,
                                    -0.062963f, -0.030270f, -0.064727f};
  RunBitexactnessTest(AudioProcessing::kSampleRate48kHz, 2, kOutputReference);
}

}  // namespace webrtc
