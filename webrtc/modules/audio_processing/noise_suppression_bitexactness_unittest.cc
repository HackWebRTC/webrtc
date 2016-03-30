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
#include "webrtc/modules/audio_processing/noise_suppression_impl.h"
#include "webrtc/modules/audio_processing/test/audio_buffer_tools.h"
#include "webrtc/modules/audio_processing/test/bitexactness_tools.h"

namespace webrtc {
namespace {

const int kNumFramesToProcess = 1000;

// Process one frame of data and produce the output.
void ProcessOneFrame(int sample_rate_hz,
                     AudioBuffer* capture_buffer,
                     NoiseSuppressionImpl* noise_suppressor) {
  if (sample_rate_hz > AudioProcessing::kSampleRate16kHz) {
    capture_buffer->SplitIntoFrequencyBands();
  }

  noise_suppressor->AnalyzeCaptureAudio(capture_buffer);
  noise_suppressor->ProcessCaptureAudio(capture_buffer);

  if (sample_rate_hz > AudioProcessing::kSampleRate16kHz) {
    capture_buffer->MergeFrequencyBands();
  }
}

// Processes a specified amount of frames, verifies the results and reports
// any errors.
void RunBitexactnessTest(int sample_rate_hz,
                         size_t num_channels,
                         NoiseSuppressionImpl::Level level,
                         float speech_probability_reference,
                         rtc::ArrayView<const float> noise_estimate_reference,
                         rtc::ArrayView<const float> output_reference) {
  rtc::CriticalSection crit_capture;
  NoiseSuppressionImpl noise_suppressor(&crit_capture);
  noise_suppressor.Initialize(num_channels, sample_rate_hz);
  noise_suppressor.Enable(true);
  noise_suppressor.set_level(level);

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

    ProcessOneFrame(sample_rate_hz, &capture_buffer, &noise_suppressor);
  }

  // Extract test results.
  std::vector<float> capture_output;
  test::ExtractVectorFromAudioBuffer(capture_config, &capture_buffer,
                                     &capture_output);
  float speech_probability = noise_suppressor.speech_probability();
  std::vector<float> noise_estimate = noise_suppressor.NoiseEstimate();

  const float kVectorElementErrorBound = 1.0f / 32768.0f;
  EXPECT_FLOAT_EQ(speech_probability_reference, speech_probability);
  EXPECT_TRUE(test::VerifyArray(noise_estimate_reference, noise_estimate,
                                kVectorElementErrorBound));

  // Compare the output with the reference. Only the first values of the output
  // from last frame processed are compared in order not having to specify all
  // preceeding frames as testvectors. As the algorithm being tested has a
  // memory, testing only the last frame implicitly also tests the preceeding
  // frames.
  EXPECT_TRUE(test::VerifyDeinterleavedArray(
      capture_config.num_frames(), capture_config.num_channels(),
      output_reference, capture_output, kVectorElementErrorBound));
}

}  // namespace

TEST(NoiseSuppresionBitExactnessTest, Mono8kHzLow) {
#if defined(WEBRTC_ARCH_ARM64)
  const float kSpeechProbabilityReference = -4.0f;
  const float kNoiseEstimateReference[] = {2.797542f, 6.488125f, 14.995160f};
  const float kOutputReference[] = {0.003510f, 0.004517f, 0.004669f};
#elif defined(WEBRTC_ARCH_ARM)
  const float kSpeechProbabilityReference = -4.0f;
  const float kNoiseEstimateReference[] = {2.797542f, 6.488125f, 14.995160f};
  const float kOutputReference[] = {0.003510f, 0.004517f, 0.004669f};
#else
  const float kSpeechProbabilityReference = 0.73421317f;
  const float kNoiseEstimateReference[] = {0.035866f, 0.100382f, 0.229889f};
  const float kOutputReference[] = {0.003263f, 0.004402f, 0.004537f};
#endif

  RunBitexactnessTest(8000, 1, NoiseSuppression::Level::kLow,
                      kSpeechProbabilityReference, kNoiseEstimateReference,
                      kOutputReference);
}

TEST(NoiseSuppresionBitExactnessTest, Mono16kHzLow) {
#if defined(WEBRTC_ARCH_ARM64)
  const float kSpeechProbabilityReference = -4.0f;
  const float kNoiseEstimateReference[] = {2.475060f, 6.130507f, 14.030761f};
  const float kOutputReference[] = {0.003449f, 0.004334f, 0.004303f};
#elif defined(WEBRTC_ARCH_ARM)
  const float kSpeechProbabilityReference = -4.0f;
  const float kNoiseEstimateReference[] = {2.475060f, 6.130507f, 14.030761f};
  const float kOutputReference[] = {0.003449f, 0.004334f, 0.004303f};
#else
  const float kSpeechProbabilityReference = 0.71672988f;
  const float kNoiseEstimateReference[] = {0.065653f, 0.198662f, 0.477870f};
  const float kOutputReference[] = {0.003574f, 0.004494f, 0.004499f};
#endif

  RunBitexactnessTest(16000, 1, NoiseSuppression::Level::kLow,
                      kSpeechProbabilityReference, kNoiseEstimateReference,
                      kOutputReference);
}

TEST(NoiseSuppresionBitExactnessTest, Mono32kHzLow) {
#if defined(WEBRTC_ARCH_ARM64)
  const float kSpeechProbabilityReference = -4.0f;
  const float kNoiseEstimateReference[] = {2.480526f, 6.169749f, 14.102388f};
  const float kOutputReference[] = {0.001679f, 0.002411f, 0.002594f};
#elif defined(WEBRTC_ARCH_ARM)
  const float kSpeechProbabilityReference = -4.0f;
  const float kNoiseEstimateReference[] = {2.480526f, 6.169749f, 14.102388f};
  const float kOutputReference[] = {0.001679f, 0.002411f, 0.002594f};
#else
  const float kSpeechProbabilityReference = 0.67999554f;
  const float kNoiseEstimateReference[] = {0.065606f, 0.215971f, 0.455931f};
  const float kOutputReference[] = {0.001221f, 0.001984f, 0.002228f};
#endif

  RunBitexactnessTest(32000, 1, NoiseSuppression::Level::kLow,
                      kSpeechProbabilityReference, kNoiseEstimateReference,
                      kOutputReference);
}

TEST(NoiseSuppresionBitExactnessTest, Mono48kHzLow) {
#if defined(WEBRTC_ARCH_ARM64)
  const float kSpeechProbabilityReference = -4.0f;
  const float kNoiseEstimateReference[] = {2.504498f, 6.068024f, 13.058871f};
  const float kOutputReference[] = {-0.013185f, -0.012769f, -0.012023f};
#elif defined(WEBRTC_ARCH_ARM)
  const float kSpeechProbabilityReference = -4.0f;
  const float kNoiseEstimateReference[] = {2.504498f, 6.068024f, 13.058871f};
  const float kOutputReference[] = {-0.013185f, -0.012769f, -0.012023f};
#else
  const float kSpeechProbabilityReference = 0.70645678f;
  const float kNoiseEstimateReference[] = {0.066186f, 0.210660f, 0.402548f};
  const float kOutputReference[] = {-0.013062f, -0.012657f, -0.011934f};
#endif

  RunBitexactnessTest(48000, 1, NoiseSuppression::Level::kLow,
                      kSpeechProbabilityReference, kNoiseEstimateReference,
                      kOutputReference);
}

TEST(NoiseSuppresionBitExactnessTest, Stereo16kHzLow) {
#if defined(WEBRTC_ARCH_ARM64)
  const float kSpeechProbabilityReference = -4.0f;
  const float kNoiseEstimateReference[] = {9.757937f, 12.392158f, 11.317673f};
  const float kOutputReference[] = {-0.011108f, -0.007904f, -0.012390f,
                                    -0.002441f, 0.000855f,  -0.003204f};
#elif defined(WEBRTC_ARCH_ARM)
  const float kSpeechProbabilityReference = -4.0f;
  const float kNoiseEstimateReference[] = {10.079447f, 11.849465f, 10.667051f};
  const float kOutputReference[] = {-0.011108f, -0.007904f, -0.012390f,
                                    -0.002472f, 0.000916f,  -0.003235f};
#else
  const float kSpeechProbabilityReference = 0.67230678f;
  const float kNoiseEstimateReference[] = {0.298195f, 0.345745f, 0.320528f};
  const float kOutputReference[] = {-0.011459f, -0.008110f, -0.012728f,
                                    -0.002399f, 0.001018f,  -0.003189f};
#endif

  RunBitexactnessTest(16000, 2, NoiseSuppression::Level::kLow,
                      kSpeechProbabilityReference, kNoiseEstimateReference,
                      kOutputReference);
}

TEST(NoiseSuppresionBitExactnessTest, Mono16kHzModerate) {
#if defined(WEBRTC_ARCH_ARM64)
  const float kSpeechProbabilityReference = -4.0f;
  const float kNoiseEstimateReference[] = {1.004436f, 3.711453f, 9.602631f};
  const float kOutputReference[] = {0.004669f, 0.005524f, 0.005432f};
#elif defined(WEBRTC_ARCH_ARM)
  const float kSpeechProbabilityReference = -4.0f;
  const float kNoiseEstimateReference[] = {1.095946f, 3.351643f, 8.167248f};
  const float kOutputReference[] = {0.004669f, 0.005615f, 0.005585f};
#else
  const float kSpeechProbabilityReference = 0.70897013f;
  const float kNoiseEstimateReference[] = {0.066269f, 0.199999f, 0.476885f};
  const float kOutputReference[] = {0.004513f, 0.005590f, 0.005614f};
#endif

  RunBitexactnessTest(16000, 1, NoiseSuppression::Level::kModerate,
                      kSpeechProbabilityReference, kNoiseEstimateReference,
                      kOutputReference);
}

TEST(NoiseSuppresionBitExactnessTest, Mono16kHzHigh) {
#if defined(WEBRTC_ARCH_ARM64)
  const float kSpeechProbabilityReference = -4.0f;
  const float kNoiseEstimateReference[] = {1.023022f, 3.759059f, 9.614030f};
  const float kOutputReference[] = {0.004639f, 0.005402f, 0.005310f};
#elif defined(WEBRTC_ARCH_ARM)
  const float kSpeechProbabilityReference = -4.0f;
  const float kNoiseEstimateReference[] = {1.114510f, 3.410356f, 8.262188f};
  const float kOutputReference[] = {0.004547f, 0.005432f, 0.005402f};
#else
  const float kSpeechProbabilityReference = 0.70106733f;
  const float kNoiseEstimateReference[] = {0.067901f, 0.204835f, 0.481723f};
  const float kOutputReference[] = {0.004394f, 0.005406f, 0.005416f};
#endif

  RunBitexactnessTest(16000, 1, NoiseSuppression::Level::kHigh,
                      kSpeechProbabilityReference, kNoiseEstimateReference,
                      kOutputReference);
}

TEST(NoiseSuppresionBitExactnessTest, Mono16kHzVeryHigh) {
#if defined(WEBRTC_ARCH_ARM64)
  const float kSpeechProbabilityReference = -4.0f;
  const float kNoiseEstimateReference[] = {2.614974f, 6.041980f, 14.029047f};
  const float kOutputReference[] = {0.004273f, 0.005127f, 0.005188f};
#elif defined(WEBRTC_ARCH_ARM)
  const float kSpeechProbabilityReference = -4.0f;
  const float kNoiseEstimateReference[] = {2.614974f, 6.041980f, 14.029047f};
  const float kOutputReference[] = {0.004273f, 0.005127f, 0.005188f};
#else
  const float kSpeechProbabilityReference = 0.70281971f;
  const float kNoiseEstimateReference[] = {0.068797f, 0.205191f, 0.481312f};
  const float kOutputReference[] = {0.004321f, 0.005247f, 0.005263f};
#endif

  RunBitexactnessTest(16000, 1, NoiseSuppression::Level::kVeryHigh,
                      kSpeechProbabilityReference, kNoiseEstimateReference,
                      kOutputReference);
}

}  // namespace webrtc
