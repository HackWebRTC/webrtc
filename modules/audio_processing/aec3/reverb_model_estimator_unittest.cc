/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/reverb_model_estimator.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/aec3_fft.h"
#include "modules/audio_processing/aec3/fft_data.h"
#include "rtc_base/checks.h"

#include "test/gtest.h"

namespace webrtc {

class ReverbModelEstimatorTest {
 public:
  explicit ReverbModelEstimatorTest(float default_decay)
      : default_decay_(default_decay),
        estimated_decay_(default_decay),
        h_(aec3_config_.filter.main.length_blocks * kBlockSize, 0.f),
        H2_(aec3_config_.filter.main.length_blocks) {
    aec3_config_.ep_strength.default_len = default_decay_;
    CreateImpulseResponseWithDecay();
  }
  void RunEstimator();
  float GetDecay() { return estimated_decay_; }
  float GetTrueDecay() { return true_power_decay_; }
  float GetPowerTailDb() { return 10.f * log10(estimated_power_tail_); }
  float GetTruePowerTailDb() { return 10.f * log10(true_power_tail_); }

 private:
  void CreateImpulseResponseWithDecay();

  absl::optional<float> quality_linear_ = 1.0f;
  static constexpr int filter_delay_blocks_ = 2;
  static constexpr bool usable_linear_estimate_ = true;
  static constexpr bool stationary_block_ = false;
  static constexpr float true_power_decay_ = 0.5f;
  EchoCanceller3Config aec3_config_;
  float default_decay_;
  float estimated_decay_;
  float estimated_power_tail_ = 0.f;
  float true_power_tail_ = 0.f;
  std::vector<float> h_;
  std::vector<std::array<float, kFftLengthBy2Plus1>> H2_;
};

void ReverbModelEstimatorTest::CreateImpulseResponseWithDecay() {
  const Aec3Fft fft;
  RTC_DCHECK_EQ(h_.size(), aec3_config_.filter.main.length_blocks * kBlockSize);
  RTC_DCHECK_EQ(H2_.size(), aec3_config_.filter.main.length_blocks);
  RTC_DCHECK_EQ(filter_delay_blocks_, 2);
  const float peak = 1.0f;
  float decay_power_sample = std::sqrt(true_power_decay_);
  for (size_t k = 1; k < kBlockSizeLog2; k++) {
    decay_power_sample = std::sqrt(decay_power_sample);
  }
  h_[filter_delay_blocks_ * kBlockSize] = peak;
  for (size_t k = filter_delay_blocks_ * kBlockSize + 1; k < h_.size(); ++k) {
    h_[k] = h_[k - 1] * std::sqrt(decay_power_sample);
  }

  for (size_t block = 0; block < H2_.size(); ++block) {
    std::array<float, kFftLength> h_block;
    h_block.fill(0.f);
    FftData H_block;
    rtc::ArrayView<float> H2_block(H2_[block]);
    std::copy(h_.begin() + block * kBlockSize,
              h_.begin() + block * (kBlockSize + 1), h_block.begin());

    fft.Fft(&h_block, &H_block);
    for (size_t k = 0; k < H2_block.size(); ++k) {
      H2_block[k] =
          H_block.re[k] * H_block.re[k] + H_block.im[k] * H_block.im[k];
    }
  }
  rtc::ArrayView<float> H2_tail(H2_[H2_.size() - 1]);
  true_power_tail_ = std::accumulate(H2_tail.begin(), H2_tail.end(), 0.f);
}
void ReverbModelEstimatorTest::RunEstimator() {
  ReverbModelEstimator estimator(aec3_config_);
  for (size_t k = 0; k < 1000; ++k) {
    estimator.Update(h_, H2_, quality_linear_, filter_delay_blocks_,
                     usable_linear_estimate_, default_decay_,
                     stationary_block_);
  }
  estimated_decay_ = estimator.ReverbDecay();
  rtc::ArrayView<const float> freq_resp_tail = estimator.GetFreqRespTail();
  estimated_power_tail_ =
      std::accumulate(freq_resp_tail.begin(), freq_resp_tail.end(), 0.f);
}

TEST(ReverbModelEstimatorTests, NotChangingDecay) {
  constexpr float default_decay = 0.9f;
  ReverbModelEstimatorTest test(default_decay);
  test.RunEstimator();
  EXPECT_EQ(test.GetDecay(), default_decay);
  EXPECT_NEAR(test.GetPowerTailDb(), test.GetTruePowerTailDb(), 5.f);
}

TEST(ReverbModelEstimatorTests, ChangingDecay) {
  constexpr float default_decay = -0.9f;
  ReverbModelEstimatorTest test(default_decay);
  test.RunEstimator();
  EXPECT_NEAR(test.GetDecay(), test.GetTrueDecay(), 0.1);
  EXPECT_NEAR(test.GetPowerTailDb(), test.GetTruePowerTailDb(), 5.f);
}

}  // namespace webrtc
