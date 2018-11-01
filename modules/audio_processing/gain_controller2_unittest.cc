/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "api/array_view.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/gain_controller2.h"
#include "modules/audio_processing/test/audio_buffer_tools.h"
#include "modules/audio_processing/test/bitexactness_tools.h"
#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

namespace {

constexpr size_t kFrameSizeMs = 10u;
constexpr size_t kStereo = 2u;

void SetAudioBufferSamples(float value, AudioBuffer* ab) {
  // Sets all the samples in |ab| to |value|.
  for (size_t k = 0; k < ab->num_channels(); ++k) {
    std::fill(ab->channels_f()[k], ab->channels_f()[k] + ab->num_frames(),
              value);
  }
}

}  // namespace

TEST(GainController2, CreateApplyConfig) {
  // Instances GainController2 and applies different configurations.
  std::unique_ptr<GainController2> gain_controller2(new GainController2());

  // Check that the default config is valid.
  AudioProcessing::Config::GainController2 config;
  EXPECT_TRUE(GainController2::Validate(config));
  gain_controller2->ApplyConfig(config);

  // Check that attenuation is not allowed.
  config.fixed_gain_db = -5.f;
  EXPECT_FALSE(GainController2::Validate(config));

  // Check that valid configurations are applied.
  for (const float& fixed_gain_db : {0.f, 5.f, 10.f, 50.f}) {
    config.fixed_gain_db = fixed_gain_db;
    EXPECT_TRUE(GainController2::Validate(config));
    gain_controller2->ApplyConfig(config);
  }
}

TEST(GainController2, ToString) {
  // Tests GainController2::ToString().
  AudioProcessing::Config::GainController2 config;
  config.fixed_gain_db = 5.f;

  config.enabled = false;
  EXPECT_EQ("{enabled: false, fixed_gain_dB: 5}",
            GainController2::ToString(config));

  config.enabled = true;
  EXPECT_EQ("{enabled: true, fixed_gain_dB: 5}",
            GainController2::ToString(config));
}

TEST(GainController2, Usage) {
  // Tests GainController2::Process() on an AudioBuffer instance.
  std::unique_ptr<GainController2> gain_controller2(new GainController2());
  gain_controller2->Initialize(AudioProcessing::kSampleRate48kHz);
  const size_t num_frames = rtc::CheckedDivExact<size_t>(
      kFrameSizeMs * AudioProcessing::kSampleRate48kHz, 1000);
  AudioBuffer ab(num_frames, kStereo, num_frames, kStereo, num_frames);
  constexpr float sample_value = 1000.f;
  SetAudioBufferSamples(sample_value, &ab);
  AudioProcessing::Config::GainController2 config;

  // Check that samples are amplified when the fixed gain is greater than 0 dB.
  config.fixed_gain_db = 5.f;
  gain_controller2->ApplyConfig(config);
  gain_controller2->Process(&ab);
  EXPECT_LT(sample_value, ab.channels_f()[0][0]);
}

float GainAfterProcessingFile(GainController2* gain_controller) {
  // Set up an AudioBuffer to be filled from the speech file.
  const StreamConfig capture_config(AudioProcessing::kSampleRate48kHz, kStereo,
                                    false);
  AudioBuffer ab(capture_config.num_frames(), capture_config.num_channels(),
                 capture_config.num_frames(), capture_config.num_channels(),
                 capture_config.num_frames());
  test::InputAudioFile capture_file(
      test::GetApmCaptureTestVectorFileName(AudioProcessing::kSampleRate48kHz));
  std::vector<float> capture_input(capture_config.num_frames() *
                                   capture_config.num_channels());

  // The file should contain at least this many frames. Every iteration, we put
  // a frame through the gain controller.
  const int kNumFramesToProcess = 100;
  for (int frame_no = 0; frame_no < kNumFramesToProcess; ++frame_no) {
    ReadFloatSamplesFromStereoFile(capture_config.num_frames(),
                                   capture_config.num_channels(), &capture_file,
                                   capture_input);

    test::CopyVectorToAudioBuffer(capture_config, capture_input, &ab);
    gain_controller->Process(&ab);
  }

  // Send in a last frame with values constant 1 (It's low enough to detect high
  // gain, and for ease of computation). The applied gain is the result.
  constexpr float sample_value = 1.f;
  SetAudioBufferSamples(sample_value, &ab);
  gain_controller->Process(&ab);
  return ab.channels_f()[0][0];
}

TEST(GainController2, UsageSaturationMargin) {
  GainController2 gain_controller2;
  gain_controller2.Initialize(AudioProcessing::kSampleRate48kHz);

  AudioProcessing::Config::GainController2 config;
  // Check that samples are not amplified as much when extra margin is
  // high. They should not be amplified at all, but anly after convergence. GC2
  // starts with a gain, and it takes time until it's down to 0db.
  config.extra_saturation_margin_db = 50.f;
  config.fixed_gain_db = 0.f;
  gain_controller2.ApplyConfig(config);

  EXPECT_LT(GainAfterProcessingFile(&gain_controller2), 2.f);
}

TEST(GainController2, UsageNoSaturationMargin) {
  GainController2 gain_controller2;
  gain_controller2.Initialize(AudioProcessing::kSampleRate48kHz);

  AudioProcessing::Config::GainController2 config;
  // Check that some gain is applied if there is no margin.
  config.extra_saturation_margin_db = 0.f;
  config.fixed_gain_db = 0.f;
  gain_controller2.ApplyConfig(config);

  EXPECT_GT(GainAfterProcessingFile(&gain_controller2), 2.f);
}
}  // namespace test
}  // namespace webrtc
