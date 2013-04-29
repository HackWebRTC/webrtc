/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/common_audio/resampler/include/push_resampler.h"

// Quality testing of PushResampler is handled through output_mixer_unittest.cc.

namespace webrtc {

typedef std::tr1::tuple<int, int, bool> PushResamplerTestData;
class PushResamplerTest
    : public testing::TestWithParam<PushResamplerTestData> {
 public:
  PushResamplerTest()
      : input_rate_(std::tr1::get<0>(GetParam())),
        output_rate_(std::tr1::get<1>(GetParam())),
        use_sinc_resampler_(std::tr1::get<2>(GetParam())) {
  }

  virtual ~PushResamplerTest() {}

 protected:
  int input_rate_;
  int output_rate_;
  bool use_sinc_resampler_;
};

TEST_P(PushResamplerTest, SincResamplerOnlyUsedWhenNecessary) {
  PushResampler resampler;
  resampler.InitializeIfNeeded(input_rate_, output_rate_, 1);
  EXPECT_EQ(use_sinc_resampler_, resampler.use_sinc_resampler());
}

INSTANTIATE_TEST_CASE_P(
    PushResamplerTest, PushResamplerTest, testing::Values(
        // To 8 kHz
        std::tr1::make_tuple(8000, 8000, false),
        std::tr1::make_tuple(16000, 8000, false),
        std::tr1::make_tuple(32000, 8000, false),
        std::tr1::make_tuple(44100, 8000, true),
        std::tr1::make_tuple(48000, 8000, false),
        std::tr1::make_tuple(96000, 8000, false),
        std::tr1::make_tuple(192000, 8000, true),

        // To 16 kHz
        std::tr1::make_tuple(8000, 16000, false),
        std::tr1::make_tuple(16000, 16000, false),
        std::tr1::make_tuple(32000, 16000, false),
        std::tr1::make_tuple(44100, 16000, true),
        std::tr1::make_tuple(48000, 16000, false),
        std::tr1::make_tuple(96000, 16000, false),
        std::tr1::make_tuple(192000, 16000, false),

        // To 32 kHz
        std::tr1::make_tuple(8000, 32000, false),
        std::tr1::make_tuple(16000, 32000, false),
        std::tr1::make_tuple(32000, 32000, false),
        std::tr1::make_tuple(44100, 32000, true),
        std::tr1::make_tuple(48000, 32000, false),
        std::tr1::make_tuple(96000, 32000, false),
        std::tr1::make_tuple(192000, 32000, false),

        // To 44.1kHz
        std::tr1::make_tuple(8000, 44100, true),
        std::tr1::make_tuple(16000, 44100, true),
        std::tr1::make_tuple(32000, 44100, true),
        std::tr1::make_tuple(44100, 44100, false),
        std::tr1::make_tuple(48000, 44100, true),
        std::tr1::make_tuple(96000, 44100, true),
        std::tr1::make_tuple(192000, 44100, true),

        // To 48kHz
        std::tr1::make_tuple(8000, 48000, false),
        std::tr1::make_tuple(16000, 48000, false),
        std::tr1::make_tuple(32000, 48000, false),
        std::tr1::make_tuple(44100, 48000, true),
        std::tr1::make_tuple(48000, 48000, false),
        std::tr1::make_tuple(96000, 48000, false),
        std::tr1::make_tuple(192000, 48000, false),

        // To 96kHz
        std::tr1::make_tuple(8000, 96000, false),
        std::tr1::make_tuple(16000, 96000, false),
        std::tr1::make_tuple(32000, 96000, false),
        std::tr1::make_tuple(44100, 96000, true),
        std::tr1::make_tuple(48000, 96000, false),
        std::tr1::make_tuple(96000, 96000, false),
        std::tr1::make_tuple(192000, 96000, false),

        // To 192kHz
        std::tr1::make_tuple(8000, 192000, true),
        std::tr1::make_tuple(16000, 192000, false),
        std::tr1::make_tuple(32000, 192000, false),
        std::tr1::make_tuple(44100, 192000, true),
        std::tr1::make_tuple(48000, 192000, false),
        std::tr1::make_tuple(96000, 192000, false),
        std::tr1::make_tuple(192000, 192000, false)));

}  // namespace webrtc
