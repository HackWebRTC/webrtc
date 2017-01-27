/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/echo_path_delay_estimator.h"

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

// Verifies that the basic API calls work.
TEST(EchoPathDelayEstimator, BasicApiCalls) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    ProduceDebugText(rate);
    ApmDataDumper data_dumper(0);
    EchoPathDelayEstimator estimator(&data_dumper, rate);
    std::vector<float> render(kBlockSize, 0.f);
    std::vector<float> capture(kBlockSize, 0.f);
    for (size_t k = 0; k < 100; ++k) {
      estimator.EstimateDelay(render, capture);
    }
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies the check for correct sample rate.
TEST(EchoPathDelayEstimator, WrongSampleRate) {
  ApmDataDumper data_dumper(0);
  EXPECT_DEATH(EchoPathDelayEstimator remover(&data_dumper, 8001), "");
}

// Verifies the check for the render blocksize.
// TODO(peah): Re-enable the test once the issue with memory leaks during DEATH
// tests on test bots has been fixed.
TEST(EchoPathDelayEstimator, DISABLED_WrongRenderBlockSize) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    ProduceDebugText(rate);
    ApmDataDumper data_dumper(0);
    EchoPathDelayEstimator estimator(&data_dumper, rate);
    std::vector<float> render(kBlockSize - 1, 0.f);
    std::vector<float> capture(kBlockSize, 0.f);
    EXPECT_DEATH(estimator.EstimateDelay(render, capture), "");
  }
}

// Verifies the check for the capture blocksize.
// TODO(peah): Re-enable the test once the issue with memory leaks during DEATH
// tests on test bots has been fixed.
TEST(EchoPathDelayEstimator, DISABLED_WrongCaptureBlockSize) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    ProduceDebugText(rate);
    ApmDataDumper data_dumper(0);
    EchoPathDelayEstimator estimator(&data_dumper, rate);
    std::vector<float> render(kBlockSize, 0.f);
    std::vector<float> capture(kBlockSize - 1, 0.f);
    EXPECT_DEATH(estimator.EstimateDelay(render, capture), "");
  }
}

// Verifies the check for non-null data dumper.
TEST(EchoPathDelayEstimator, NullDataDumper) {
  EXPECT_DEATH(EchoPathDelayEstimator(nullptr, 8000), "");
}

#endif

}  // namespace webrtc
