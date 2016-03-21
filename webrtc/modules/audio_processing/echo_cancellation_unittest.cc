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
#include "webrtc/modules/audio_processing/echo_cancellation_impl.h"
#include "webrtc/modules/audio_processing/test/audio_buffer_tools.h"
#include "webrtc/modules/audio_processing/test/bitexactness_tools.h"

namespace webrtc {
namespace {

const int kNumFramesToProcess = 1000;

void SetupComponent(int sample_rate_hz,
                    EchoCancellation::SuppressionLevel suppression_level,
                    bool drift_compensation_enabled,
                    EchoCancellationImpl* echo_canceller) {
  echo_canceller->Initialize(sample_rate_hz, 1, 1, 1);
  EchoCancellation* ec = static_cast<EchoCancellation*>(echo_canceller);
  ec->Enable(true);
  ec->set_suppression_level(suppression_level);
  ec->enable_drift_compensation(drift_compensation_enabled);

  Config config;
  config.Set<DelayAgnostic>(new DelayAgnostic(true));
  config.Set<ExtendedFilter>(new ExtendedFilter(true));
  echo_canceller->SetExtraOptions(config);
}

void ProcessOneFrame(int sample_rate_hz,
                     int stream_delay_ms,
                     bool drift_compensation_enabled,
                     int stream_drift_samples,
                     AudioBuffer* render_audio_buffer,
                     AudioBuffer* capture_audio_buffer,
                     EchoCancellationImpl* echo_canceller) {
  if (sample_rate_hz > AudioProcessing::kSampleRate16kHz) {
    render_audio_buffer->SplitIntoFrequencyBands();
    capture_audio_buffer->SplitIntoFrequencyBands();
  }

  echo_canceller->ProcessRenderAudio(render_audio_buffer);

  if (drift_compensation_enabled) {
    static_cast<EchoCancellation*>(echo_canceller)
        ->set_stream_drift_samples(stream_drift_samples);
  }

  echo_canceller->ProcessCaptureAudio(capture_audio_buffer, stream_delay_ms);

  if (sample_rate_hz > AudioProcessing::kSampleRate16kHz) {
    capture_audio_buffer->MergeFrequencyBands();
  }
}

void RunBitexactnessTest(int sample_rate_hz,
                         size_t num_channels,
                         int stream_delay_ms,
                         bool drift_compensation_enabled,
                         int stream_drift_samples,
                         EchoCancellation::SuppressionLevel suppression_level,
                         bool stream_has_echo_reference,
                         const rtc::ArrayView<const float>& output_reference) {
  rtc::CriticalSection crit_render;
  rtc::CriticalSection crit_capture;
  EchoCancellationImpl echo_canceller(&crit_render, &crit_capture);
  SetupComponent(sample_rate_hz, suppression_level, drift_compensation_enabled,
                 &echo_canceller);

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

    ProcessOneFrame(sample_rate_hz, stream_delay_ms, drift_compensation_enabled,
                    stream_drift_samples, &render_buffer, &capture_buffer,
                    &echo_canceller);
  }

  // Extract and verify the test results.
  std::vector<float> capture_output;
  test::ExtractVectorFromAudioBuffer(capture_config, &capture_buffer,
                                     &capture_output);

  EXPECT_EQ(stream_has_echo_reference,
            static_cast<EchoCancellation*>(&echo_canceller)->stream_has_echo());

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

const bool kStreamHasEchoReference = false;

}  // namespace

TEST(EchoCancellationBitExactnessTest,
     Mono8kHz_HighLevel_NoDrift_StreamDelay0) {
#if defined(WEBRTC_ARCH_ARM64)
  const float kOutputReference[] = {0.005061f, 0.009174f, 0.012192f};
#elif defined(WEBRTC_ARCH_ARM)
  const float kOutputReference[] = {0.005061f, 0.009174f, 0.012192f};
#else
  const float kOutputReference[] = {0.005739f, 0.009969f, 0.013096f};
#endif

  RunBitexactnessTest(8000, 1, 0, false, 0,
                      EchoCancellation::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

TEST(EchoCancellationBitExactnessTest,
     Mono16kHz_HighLevel_NoDrift_StreamDelay0) {
#if defined(WEBRTC_ARCH_ARM64)
  const float kOutputReference[] = {-0.017961f, -0.016535f, -0.014739f};
#elif defined(WEBRTC_ARCH_ARM)
  const float kOutputReference[] = {-0.017961f, -0.016535f, -0.014739f};
#else
  const float kOutputReference[] = {-0.017875f, -0.016454f, -0.014657f};
#endif
  RunBitexactnessTest(16000, 1, 0, false, 0,
                      EchoCancellation::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

TEST(EchoCancellationBitExactnessTest,
     Mono32kHz_HighLevel_NoDrift_StreamDelay0) {
#if defined(WEBRTC_ARCH_ARM64)
  const float kOutputReference[] = {-0.020325f, -0.020111f, -0.019165f};
#elif defined(WEBRTC_ARCH_ARM)
  const float kOutputReference[] = {-0.020325f, -0.020111f, -0.019165f};
#elif defined(WEBRTC_MAC)
  const bool kStreamHasEchoReference = false;
  const float kOutputReference[] = {-0.020111f, -0.019958f, -0.019012f};
#else
  const bool kStreamHasEchoReference = false;
  const float kOutputReference[] = {-0.020294f, -0.020081f, -0.019135f};
#endif

  RunBitexactnessTest(32000, 1, 0, false, 0,
                      EchoCancellation::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

TEST(EchoCancellationBitExactnessTest,
     Mono48kHz_HighLevel_NoDrift_StreamDelay0) {
#if defined(WEBRTC_ARCH_ARM64)
  const float kOutputReference[] = {-0.016424f, -0.016843f, -0.017117f};
#elif defined(WEBRTC_ARCH_ARM)
  const float kOutputReference[] = {-0.016424f, -0.016843f, -0.017117f};
#else
  const float kOutputReference[] = {-0.016347f, -0.016763f, -0.017036f};
#endif
  RunBitexactnessTest(48000, 1, 0, false, 0,
                      EchoCancellation::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

TEST(EchoCancellationBitExactnessTest,
     Mono16kHz_LowLevel_NoDrift_StreamDelay0) {
#if defined(WEBRTC_ARCH_ARM64)
  const float kOutputReference[] = {-0.018348f, -0.016953f, -0.015167f};
#elif defined(WEBRTC_ARCH_ARM)
  const float kOutputReference[] = {-0.018348f, -0.016953f, -0.015167f};
#else
  const float kOutputReference[] = {-0.018289f, -0.016901f, -0.015122f};
#endif

  RunBitexactnessTest(16000, 1, 0, false, 0,
                      EchoCancellation::SuppressionLevel::kLowSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

TEST(EchoCancellationBitExactnessTest,
     Mono16kHz_ModerateLevel_NoDrift_StreamDelay0) {
#if defined(WEBRTC_ARCH_ARM64)
  const float kOutputReference[] = {-0.018253f, -0.016845f, -0.015055f};
#elif defined(WEBRTC_ARCH_ARM)
  const float kOutputReference[] = {-0.018253f, -0.016845f, -0.015055f};
#else
  const bool kStreamHasEchoReference = false;
  const float kOutputReference[] = {-0.018194f, -0.016788f, -0.014997f};
#endif
  RunBitexactnessTest(16000, 1, 0, false, 0,
                      EchoCancellation::SuppressionLevel::kModerateSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

TEST(EchoCancellationBitExactnessTest,
     Mono16kHz_HighLevel_NoDrift_StreamDelay10) {
#if defined(WEBRTC_ARCH_ARM64)
  const float kOutputReference[] = {-0.017961f, -0.016535f, -0.014739f};
#elif defined(WEBRTC_ARCH_ARM)
  const float kOutputReference[] = {-0.017961f, -0.016535f, -0.014739f};
#else
  const float kOutputReference[] = {-0.017875f, -0.016454f, -0.014657f};
#endif
  RunBitexactnessTest(16000, 1, 10, false, 0,
                      EchoCancellation::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

TEST(EchoCancellationBitExactnessTest,
     Mono16kHz_HighLevel_NoDrift_StreamDelay20) {
#if defined(WEBRTC_ARCH_ARM64)
  const float kOutputReference[] = {-0.017961f, -0.016535f, -0.014739f};
#elif defined(WEBRTC_ARCH_ARM)
  const float kOutputReference[] = {-0.017961f, -0.016535f, -0.014739f};
#else
  const float kOutputReference[] = {-0.017875f, -0.016454f, -0.014657f};
#endif
  RunBitexactnessTest(16000, 1, 20, false, 0,
                      EchoCancellation::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

TEST(EchoCancellationBitExactnessTest,
     Mono16kHz_HighLevel_Drift0_StreamDelay0) {
#if defined(WEBRTC_ARCH_ARM64)
  const float kOutputReference[] = {-0.017961f, -0.016535f, -0.014739f};
#elif defined(WEBRTC_ARCH_ARM)
  const float kOutputReference[] = {-0.017961f, -0.016535f, -0.014739f};
#else
  const float kOutputReference[] = {-0.017875f, -0.016454f, -0.014657f};
#endif
  RunBitexactnessTest(16000, 1, 0, true, 0,
                      EchoCancellation::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

TEST(EchoCancellationBitExactnessTest,
     Mono16kHz_HighLevel_Drift5_StreamDelay0) {
#if defined(WEBRTC_ARCH_ARM64)
  const float kOutputReference[] = {-0.017961f, -0.016535f, -0.014739f};
#elif defined(WEBRTC_ARCH_ARM)
  const float kOutputReference[] = {-0.017961f, -0.016535f, -0.014739f};
#else
  const float kOutputReference[] = {-0.017875f, -0.016454f, -0.014657f};
#endif

  RunBitexactnessTest(16000, 1, 0, true, 5,
                      EchoCancellation::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

TEST(EchoCancellationBitExactnessTest,
     Stereo8kHz_HighLevel_NoDrift_StreamDelay0) {
#if defined(WEBRTC_ARCH_ARM64)
  const float kOutputReference[] = {0.011901f, 0.004306f, 0.010258f,
                                    0.011901f, 0.004306f, 0.010258f};
#elif defined(WEBRTC_ARCH_ARM)
  const float kOutputReference[] = {0.011900f, 0.004306f, 0.010258f,
                                    0.011900f, 0.004306f, 0.010258f};
#else
  const float kOutputReference[] = {0.011691f, 0.004257f, 0.010092f,
                                    0.011691f, 0.004257f, 0.010092f};
#endif
  RunBitexactnessTest(8000, 2, 0, false, 0,
                      EchoCancellation::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

TEST(EchoCancellationBitExactnessTest,
     Stereo16kHz_HighLevel_NoDrift_StreamDelay0) {
#if defined(WEBRTC_ARCH_ARM64)
  const float kOutputReference[] = {0.000840f, 0.006285f, -0.000440f,
                                    0.000840f, 0.006285f, -0.000440f};
#elif defined(WEBRTC_ARCH_ARM)
  const float kOutputReference[] = {0.000840f, 0.006285f, -0.000440f,
                                    0.000840f, 0.006285f, -0.000440f};
#else
  const float kOutputReference[] = {0.000677f, 0.006431f, -0.000613f,
                                    0.000677f, 0.006431f, -0.000613f};
#endif
  RunBitexactnessTest(16000, 2, 0, false, 0,
                      EchoCancellation::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

TEST(EchoCancellationBitExactnessTest,
     Stereo32kHz_HighLevel_NoDrift_StreamDelay0) {
#if defined(WEBRTC_ARCH_ARM64)
  const float kOutputReference[] = {0.001556f, 0.007599f, 0.001068f,
                                    0.001556f, 0.007599f, 0.001068f};
#elif defined(WEBRTC_ARCH_ARM)
  const float kOutputReference[] = {0.001556f, 0.007599f, 0.001068f,
                                    0.001556f, 0.007599f, 0.001068f};
#else
  const float kOutputReference[] = {0.001526f, 0.007630f, 0.001007f,
                                    0.001526f, 0.007630f, 0.001007f};
#endif
  RunBitexactnessTest(32000, 2, 0, false, 0,
                      EchoCancellation::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

TEST(EchoCancellationBitExactnessTest,
     Stereo48kHz_HighLevel_NoDrift_StreamDelay0) {
#if defined(WEBRTC_ARCH_ARM64)
  const float kOutputReference[] = {0.004406f, 0.011327f, 0.004271f,
                                    0.004406f, 0.011327f, 0.004271f};
#elif defined(WEBRTC_ARCH_ARM)
  const float kOutputReference[] = {0.004406f, 0.011327f, 0.004271f,
                                    0.004406f, 0.011327f, 0.004271f};
#else
  const float kOutputReference[] = {0.004390f, 0.011286f, 0.004254f,
                                    0.004390f, 0.011286f, 0.004254f};
#endif
  RunBitexactnessTest(48000, 2, 0, false, 0,
                      EchoCancellation::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

}  // namespace webrtc
