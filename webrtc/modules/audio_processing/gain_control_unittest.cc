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
#include "webrtc/modules/audio_processing/gain_control_impl.h"
#include "webrtc/modules/audio_processing/test/audio_buffer_tools.h"
#include "webrtc/modules/audio_processing/test/bitexactness_tools.h"

namespace webrtc {
namespace {

const int kNumFramesToProcess = 1000;

void ProcessOneFrame(int sample_rate_hz,
                     AudioBuffer* render_audio_buffer,
                     AudioBuffer* capture_audio_buffer,
                     GainControlImpl* gain_controller) {
  if (sample_rate_hz > AudioProcessing::kSampleRate16kHz) {
    render_audio_buffer->SplitIntoFrequencyBands();
    capture_audio_buffer->SplitIntoFrequencyBands();
  }

  gain_controller->ProcessRenderAudio(render_audio_buffer);
  gain_controller->AnalyzeCaptureAudio(capture_audio_buffer);
  gain_controller->ProcessCaptureAudio(capture_audio_buffer, false);

  if (sample_rate_hz > AudioProcessing::kSampleRate16kHz) {
    capture_audio_buffer->MergeFrequencyBands();
  }
}

void SetupComponent(int sample_rate_hz,
                    GainControl::Mode mode,
                    int target_level_dbfs,
                    int stream_analog_level,
                    int compression_gain_db,
                    bool enable_limiter,
                    int analog_level_min,
                    int analog_level_max,
                    GainControlImpl* gain_controller) {
  gain_controller->Initialize(1, sample_rate_hz);
  GainControl* gc = static_cast<GainControl*>(gain_controller);
  gc->Enable(true);
  gc->set_mode(mode);
  gc->set_stream_analog_level(stream_analog_level);
  gc->set_target_level_dbfs(target_level_dbfs);
  gc->set_compression_gain_db(compression_gain_db);
  gc->enable_limiter(enable_limiter);
  gc->set_analog_level_limits(analog_level_min, analog_level_max);
}

void RunBitExactnessTest(int sample_rate_hz,
                         size_t num_channels,
                         GainControl::Mode mode,
                         int target_level_dbfs,
                         int stream_analog_level,
                         int compression_gain_db,
                         bool enable_limiter,
                         int analog_level_min,
                         int analog_level_max,
                         int achieved_stream_analog_level_reference,
                         rtc::ArrayView<const float> output_reference) {
  rtc::CriticalSection crit_render;
  rtc::CriticalSection crit_capture;
  GainControlImpl gain_controller(&crit_render, &crit_capture);
  SetupComponent(sample_rate_hz, mode, target_level_dbfs, stream_analog_level,
                 compression_gain_db, enable_limiter, analog_level_min,
                 analog_level_max, &gain_controller);

  const int samples_per_channel = rtc::CheckedDivExact(sample_rate_hz, 100);
  const StreamConfig render_config(sample_rate_hz, num_channels, false);
  AudioBuffer render_buffer(
      render_config.num_frames(), render_config.num_channels(),
      render_config.num_frames(), 1, render_config.num_frames());
  test::InputAudioFile render_file(
      test::GetApmRenderTestVectorFileName(sample_rate_hz));
  std::vector<float> render_input(samples_per_channel * num_channels);

  const StreamConfig capture_config(sample_rate_hz, num_channels, false);
  AudioBuffer capture_buffer(
      capture_config.num_frames(), capture_config.num_channels(),
      capture_config.num_frames(), 1, capture_config.num_frames());
  test::InputAudioFile capture_file(
      test::GetApmCaptureTestVectorFileName(sample_rate_hz));
  std::vector<float> capture_input(samples_per_channel * num_channels);

  for (int frame_no = 0; frame_no < kNumFramesToProcess; ++frame_no) {
    ReadFloatSamplesFromStereoFile(samples_per_channel, num_channels,
                                   &render_file, render_input);
    ReadFloatSamplesFromStereoFile(samples_per_channel, num_channels,
                                   &capture_file, capture_input);

    test::CopyVectorToAudioBuffer(render_config, render_input, &render_buffer);
    test::CopyVectorToAudioBuffer(capture_config, capture_input,
                                  &capture_buffer);

    ProcessOneFrame(sample_rate_hz, &render_buffer, &capture_buffer,
                    &gain_controller);
  }

  // Extract and verify the test results.
  std::vector<float> capture_output;
  test::ExtractVectorFromAudioBuffer(capture_config, &capture_buffer,
                                     &capture_output);

  EXPECT_EQ(achieved_stream_analog_level_reference,
            gain_controller.stream_analog_level());

  // Compare the output with the reference. Only the first values of the output
  // from last frame processed are compared in order not having to specify all
  // preceeding frames as testvectors. As the algorithm being tested has a
  // memory, testing only the last frame implicitly also tests the preceeding
  // frames.
  const float kTolerance = 1.0f / 32768.0f;
  EXPECT_TRUE(test::BitExactFrame(
      capture_config.num_frames(), capture_config.num_channels(),
      output_reference, capture_output, kTolerance));
}

}  // namespace

TEST(GainControlBitExactnessTest,
     Mono8kHz_AdaptiveAnalog_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.004578f, -0.003998f, -0.002991f};
  RunBitExactnessTest(8000, 1, GainControl::Mode::kAdaptiveAnalog, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     Mono16kHz_AdaptiveAnalog_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.004303f, -0.004150f, -0.004089f};
  RunBitExactnessTest(16000, 1, GainControl::Mode::kAdaptiveAnalog, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     Stereo16kHz_AdaptiveAnalog_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.010254f, -0.004761f, -0.009918f,
                                    -0.010254f, -0.004761f, -0.009918f};
  RunBitExactnessTest(16000, 2, GainControl::Mode::kAdaptiveAnalog, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     Mono32kHz_AdaptiveAnalog_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.005554f, -0.005066f, -0.004242f};
  RunBitExactnessTest(32000, 1, GainControl::Mode::kAdaptiveAnalog, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     Mono48kHz_AdaptiveAnalog_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.005554f, -0.005066f, -0.004242f};
  RunBitExactnessTest(32000, 1, GainControl::Mode::kAdaptiveAnalog, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     Mono8kHz_AdaptiveDigital_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.014221f, -0.012421f, -0.009308f};
  RunBitExactnessTest(8000, 1, GainControl::Mode::kAdaptiveDigital, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     Mono16kHz_AdaptiveDigital_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.014923f, -0.014404f, -0.014191f};
  RunBitExactnessTest(16000, 1, GainControl::Mode::kAdaptiveDigital, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     Stereo16kHz_AdaptiveDigital_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.009796f, -0.004547f, -0.009460f,
                                    -0.009796f, -0.004547f, -0.009460f};
  RunBitExactnessTest(16000, 2, GainControl::Mode::kAdaptiveDigital, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     Mono32kHz_AdaptiveDigital_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.019287f, -0.017578f, -0.014709f};
  RunBitExactnessTest(32000, 1, GainControl::Mode::kAdaptiveDigital, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     Mono48kHz_AdaptiveDigital_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.019287f, -0.017578f, -0.014709f};
  RunBitExactnessTest(32000, 1, GainControl::Mode::kAdaptiveDigital, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     Mono8kHz_FixedDigital_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.008209f, -0.007172f, -0.005371f};
  RunBitExactnessTest(8000, 1, GainControl::Mode::kFixedDigital, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     Mono16kHz_FixedDigital_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.007721f, -0.007446f, -0.007355f};
  RunBitExactnessTest(16000, 1, GainControl::Mode::kFixedDigital, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     Stereo16kHz_FixedDigital_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.018402f, -0.008545f, -0.017792f,
                                    -0.018402f, -0.008545f, -0.017792f};
  RunBitExactnessTest(16000, 2, GainControl::Mode::kFixedDigital, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     Mono32kHz_FixedDigital_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.009979f, -0.009064f, -0.007629f};
  RunBitExactnessTest(32000, 1, GainControl::Mode::kFixedDigital, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     Mono48kHz_FixedDigital_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.009979f, -0.009064f, -0.007629f};
  RunBitExactnessTest(32000, 1, GainControl::Mode::kFixedDigital, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     Mono16kHz_AdaptiveAnalog_Tl10_SL10_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 12;
  const float kOutputReference[] = {-0.004303f, -0.004150f, -0.004089f};
  RunBitExactnessTest(16000, 1, GainControl::Mode::kAdaptiveAnalog, 10, 10, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     Mono16kHz_AdaptiveAnalog_Tl10_SL100_CG5_Lim_AL70_80) {
  const int kStreamAnalogLevelReference = 100;
  const float kOutputReference[] = {-0.004303f, -0.004150f, -0.004089f};
  RunBitExactnessTest(16000, 1, GainControl::Mode::kAdaptiveAnalog, 10, 100, 5,
                      true, 70, 80, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     Mono16kHz_AdaptiveDigital_Tl10_SL100_CG5_NoLim_AL0_100) {
  const int kStreamAnalogLevelReference = 100;
  const float kOutputReference[] = {-0.014923f, -0.014404f, -0.014191f};
  RunBitExactnessTest(16000, 1, GainControl::Mode::kAdaptiveDigital, 10, 100, 5,
                      false, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     Mono16kHz_AdaptiveDigital_Tl40_SL100_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 100;
  const float kOutputReference[] = {-0.020721f, -0.019989f, -0.019714f};
  RunBitExactnessTest(16000, 1, GainControl::Mode::kAdaptiveDigital, 40, 100, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     Mono16kHz_AdaptiveDigital_Tl10_SL100_CG30_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 100;
  const float kOutputReference[] = {-0.020416f, -0.019714f, -0.019409f};
  RunBitExactnessTest(16000, 1, GainControl::Mode::kAdaptiveDigital, 10, 100,
                      30, true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

}  // namespace webrtc
