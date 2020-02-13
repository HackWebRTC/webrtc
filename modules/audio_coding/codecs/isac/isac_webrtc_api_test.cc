/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <array>
#include <limits>
#include <vector>

#include "api/array_view.h"
#include "api/audio_codecs/isac/audio_decoder_isac_fix.h"
#include "api/audio_codecs/isac/audio_decoder_isac_float.h"
#include "api/audio_codecs/isac/audio_encoder_isac_fix.h"
#include "api/audio_codecs/isac/audio_encoder_isac_float.h"
#include "rtc_base/random.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr int kPayloadType = 42;
constexpr int kBitrateBps = 20000;

enum class IsacImpl { kFixed, kFloat };

std::vector<int16_t> GetRandomSamplesVector(size_t size) {
  constexpr int32_t kMin = std::numeric_limits<int16_t>::min();
  constexpr int32_t kMax = std::numeric_limits<int16_t>::max();
  std::vector<int16_t> v(size);
  Random gen(/*seed=*/42);
  for (auto& x : v) {
    x = static_cast<int16_t>(gen.Rand(kMin, kMax));
  }
  return v;
}

class IsacApiTest
    : public testing::TestWithParam<std::tuple<int, int, IsacImpl, IsacImpl>> {
 protected:
  IsacApiTest() : input_frame_(GetRandomSamplesVector(GetInputFrameLength())) {}
  rtc::ArrayView<const int16_t> GetInputFrame() { return input_frame_; }
  int GetSampleRateHz() const { return std::get<0>(GetParam()); }
  int GetEncoderFrameLenght() const {
    return GetEncoderFrameLenghtMs() * GetSampleRateHz() / 1000;
  }
  std::unique_ptr<AudioEncoder> CreateEncoder() const {
    switch (GetEncoderIsacImpl()) {
      case IsacImpl::kFixed: {
        AudioEncoderIsacFix::Config config;
        config.frame_size_ms = GetEncoderFrameLenghtMs();
        RTC_CHECK_EQ(16000, GetSampleRateHz());
        return AudioEncoderIsacFix::MakeAudioEncoder(config, kPayloadType);
      }
      case IsacImpl::kFloat: {
        AudioEncoderIsacFloat::Config config;
        config.bit_rate = kBitrateBps;
        config.frame_size_ms = GetEncoderFrameLenghtMs();
        config.sample_rate_hz = GetSampleRateHz();
        return AudioEncoderIsacFloat::MakeAudioEncoder(config, kPayloadType);
      }
    }
  }
  std::unique_ptr<AudioDecoder> CreateDecoder() const {
    switch (GetDecoderIsacImpl()) {
      case IsacImpl::kFixed: {
        webrtc::AudioDecoderIsacFix::Config config;
        RTC_CHECK_EQ(16000, GetSampleRateHz());
        return webrtc::AudioDecoderIsacFix::MakeAudioDecoder(config);
      }
      case IsacImpl::kFloat: {
        webrtc::AudioDecoderIsacFloat::Config config;
        config.sample_rate_hz = GetSampleRateHz();
        return webrtc::AudioDecoderIsacFloat::MakeAudioDecoder(config);
      }
    }
  }

 private:
  const std::vector<int16_t> input_frame_;
  int GetInputFrameLength() const {
    return rtc::CheckedDivExact(std::get<0>(GetParam()), 100);  // 10 ms.
  }
  int GetEncoderFrameLenghtMs() const {
    int frame_size_ms = std::get<1>(GetParam());
    RTC_CHECK(frame_size_ms == 30 || frame_size_ms == 60);
    return frame_size_ms;
  }
  IsacImpl GetEncoderIsacImpl() const { return std::get<2>(GetParam()); }
  IsacImpl GetDecoderIsacImpl() const { return std::get<3>(GetParam()); }
};

// Checks that the number of encoded and decoded samples match.
TEST_P(IsacApiTest, EncodeDecode) {
  auto encoder = CreateEncoder();
  auto decoder = CreateDecoder();
  const int encoder_frame_length = GetEncoderFrameLenght();
  std::vector<int16_t> out(encoder_frame_length);
  size_t num_encoded_samples = 0;
  size_t num_decoded_samples = 0;
  constexpr int kNumFrames = 12;
  for (int i = 0; i < kNumFrames; ++i) {
    rtc::Buffer encoded;
    auto in = GetInputFrame();
    encoder->Encode(/*rtp_timestamp=*/0, in, &encoded);
    num_encoded_samples += in.size();
    if (encoded.empty()) {
      continue;
    }
    // Decode.
    const std::vector<AudioDecoder::ParseResult> parse_result =
        decoder->ParsePayload(std::move(encoded), /*timestamp=*/0);
    EXPECT_EQ(parse_result.size(), size_t{1});
    auto decode_result = parse_result[0].frame->Decode(out);
    EXPECT_TRUE(decode_result.has_value());
    EXPECT_EQ(out.size(), decode_result->num_decoded_samples);
    num_decoded_samples += decode_result->num_decoded_samples;
  }
  EXPECT_EQ(num_encoded_samples, num_decoded_samples);
}

// Creates tests for different encoder frame lengths and different
// encoder/decoder implementations.
INSTANTIATE_TEST_SUITE_P(
    AllTest,
    IsacApiTest,
    ::testing::ValuesIn([] {
      std::vector<std::tuple<int, int, IsacImpl, IsacImpl>> cases;
      for (int frame_length_ms : {30, 60}) {
        for (IsacImpl enc : {IsacImpl::kFloat, IsacImpl::kFixed}) {
          for (IsacImpl dec : {IsacImpl::kFloat, IsacImpl::kFixed}) {
            cases.push_back({16000, frame_length_ms, enc, dec});
          }
        }
      }
      cases.push_back({32000, 30, IsacImpl::kFloat, IsacImpl::kFloat});
      return cases;
    }()));

}  // namespace
}  // namespace webrtc
