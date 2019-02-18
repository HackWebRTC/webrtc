/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <cmath>
#include <vector>

#include "modules/audio_processing/agc2/rnn_vad/common.h"
#include "modules/audio_processing/agc2/rnn_vad/fft_util.h"
#include "rtc_base/checks.h"
// TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
// #include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace rnn_vad {
namespace test {
namespace {

std::vector<float> CreateSine(float amplitude,
                              float frequency_hz,
                              float duration_s,
                              int sample_rate_hz) {
  size_t num_samples = static_cast<size_t>(duration_s * sample_rate_hz);
  std::vector<float> signal(num_samples);
  for (size_t i = 0; i < num_samples; ++i) {
    signal[i] =
        amplitude * std::sin(i * 2.0 * kPi * frequency_hz / sample_rate_hz);
  }
  return signal;
}

}  // namespace

TEST(RnnVadTest, BandAnalysisFftTest) {
  for (float frequency_hz : {200.f, 450.f, 1500.f}) {
    SCOPED_TRACE(frequency_hz);
    auto x = CreateSine(
        /*amplitude=*/1000.f, frequency_hz,
        /*duration_s=*/0.02f,
        /*sample_rate_hz=*/kSampleRate24kHz);
    BandAnalysisFft analyzer;
    std::vector<std::complex<float>> x_fft(x.size() / 2 + 1);
    analyzer.ForwardFft(x, x_fft);
    int peak_fft_bin_index = std::distance(
        x_fft.begin(),
        std::max_element(x_fft.begin(), x_fft.end(),
                         [](std::complex<float> a, std::complex<float> b) {
                           return std::abs(a) < std::abs(b);
                         }));
    EXPECT_EQ(frequency_hz, kSampleRate24kHz * peak_fft_bin_index / x.size());
  }
}

}  // namespace test
}  // namespace rnn_vad
}  // namespace webrtc
