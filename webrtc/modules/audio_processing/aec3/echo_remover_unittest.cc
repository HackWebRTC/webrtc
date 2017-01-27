/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/echo_remover.h"

#include <memory>
#include <sstream>
#include <string>

#include "webrtc/modules/audio_processing/aec3/aec3_constants.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace {

std::string ProduceDebugText(int sample_rate_hz) {
  std::ostringstream ss;
  ss << "Sample rate: " << sample_rate_hz;
  return ss.str();
}

}  // namespace

// Verifies the basic API call sequence
TEST(EchoRemover, BasicApiCalls) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    ProduceDebugText(rate);
    std::unique_ptr<EchoRemover> remover(EchoRemover::Create(rate));

    std::vector<std::vector<float>> render(NumBandsForRate(rate),
                                           std::vector<float>(kBlockSize, 0.f));
    std::vector<std::vector<float>> capture(
        NumBandsForRate(rate), std::vector<float>(kBlockSize, 0.f));
    for (size_t k = 0; k < 100; ++k) {
      EchoPathVariability echo_path_variability(k % 3 == 0 ? true : false,
                                                k % 5 == 0 ? true : false);
      rtc::Optional<size_t> echo_path_delay_samples =
          (k % 6 == 0 ? rtc::Optional<size_t>(k * 10)
                      : rtc::Optional<size_t>());
      remover->ProcessBlock(echo_path_delay_samples, echo_path_variability,
                            k % 2 == 0 ? true : false, render, &capture);
      remover->UpdateEchoLeakageStatus(k % 7 == 0 ? true : false);
    }
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies the check for the samplerate.
// TODO(peah): Re-enable the test once the issue with memory leaks during DEATH
// tests on test bots has been fixed.
TEST(EchoRemover, DISABLED_WrongSampleRate) {
  EXPECT_DEATH(std::unique_ptr<EchoRemover>(EchoRemover::Create(8001)), "");
}

// Verifies the check for the render block size.
TEST(EchoRemover, WrongRenderBlockSize) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    ProduceDebugText(rate);
    std::unique_ptr<EchoRemover> remover(EchoRemover::Create(rate));

    std::vector<std::vector<float>> render(
        NumBandsForRate(rate), std::vector<float>(kBlockSize - 1, 0.f));
    std::vector<std::vector<float>> capture(
        NumBandsForRate(rate), std::vector<float>(kBlockSize, 0.f));
    EchoPathVariability echo_path_variability(false, false);
    rtc::Optional<size_t> echo_path_delay_samples;
    EXPECT_DEATH(
        remover->ProcessBlock(echo_path_delay_samples, echo_path_variability,
                              false, render, &capture),
        "");
  }
}

// Verifies the check for the capture block size.
TEST(EchoRemover, WrongCaptureBlockSize) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    ProduceDebugText(rate);
    std::unique_ptr<EchoRemover> remover(EchoRemover::Create(rate));

    std::vector<std::vector<float>> render(NumBandsForRate(rate),
                                           std::vector<float>(kBlockSize, 0.f));
    std::vector<std::vector<float>> capture(
        NumBandsForRate(rate), std::vector<float>(kBlockSize - 1, 0.f));
    EchoPathVariability echo_path_variability(false, false);
    rtc::Optional<size_t> echo_path_delay_samples;
    EXPECT_DEATH(
        remover->ProcessBlock(echo_path_delay_samples, echo_path_variability,
                              false, render, &capture),
        "");
  }
}

// Verifies the check for the number of render bands.
TEST(EchoRemover, WrongRenderNumBands) {
  for (auto rate : {16000, 32000, 48000}) {
    ProduceDebugText(rate);
    std::unique_ptr<EchoRemover> remover(EchoRemover::Create(rate));

    std::vector<std::vector<float>> render(
        NumBandsForRate(rate == 48000 ? 16000 : rate + 16000),
        std::vector<float>(kBlockSize, 0.f));
    std::vector<std::vector<float>> capture(
        NumBandsForRate(rate), std::vector<float>(kBlockSize, 0.f));
    EchoPathVariability echo_path_variability(false, false);
    rtc::Optional<size_t> echo_path_delay_samples;
    EXPECT_DEATH(
        remover->ProcessBlock(echo_path_delay_samples, echo_path_variability,
                              false, render, &capture),
        "");
  }
}

// Verifies the check for the number of capture bands.
TEST(EchoRemover, WrongCaptureNumBands) {
  for (auto rate : {16000, 32000, 48000}) {
    ProduceDebugText(rate);
    std::unique_ptr<EchoRemover> remover(EchoRemover::Create(rate));

    std::vector<std::vector<float>> render(NumBandsForRate(rate),
                                           std::vector<float>(kBlockSize, 0.f));
    std::vector<std::vector<float>> capture(
        NumBandsForRate(rate == 48000 ? 16000 : rate + 16000),
        std::vector<float>(kBlockSize, 0.f));
    EchoPathVariability echo_path_variability(false, false);
    rtc::Optional<size_t> echo_path_delay_samples;
    EXPECT_DEATH(
        remover->ProcessBlock(echo_path_delay_samples, echo_path_variability,
                              false, render, &capture),
        "");
  }
}

// Verifies the check for non-null capture block.
TEST(EchoRemover, NullCapture) {
  std::unique_ptr<EchoRemover> remover(EchoRemover::Create(8000));

  std::vector<std::vector<float>> render(NumBandsForRate(8000),
                                         std::vector<float>(kBlockSize, 0.f));
  EchoPathVariability echo_path_variability(false, false);
  rtc::Optional<size_t> echo_path_delay_samples;
  EXPECT_DEATH(
      remover->ProcessBlock(echo_path_delay_samples, echo_path_variability,
                            false, render, nullptr),
      "");
}

#endif

}  // namespace webrtc
