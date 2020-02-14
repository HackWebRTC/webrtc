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

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/audio_codecs/isac/audio_decoder_isac_fix.h"
#include "api/audio_codecs/isac/audio_decoder_isac_float.h"
#include "api/audio_codecs/isac/audio_encoder_isac_fix.h"
#include "api/audio_codecs/isac/audio_encoder_isac_float.h"
#include "rtc_base/random.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr int kPayloadType = 42;

enum class IsacImpl { kFixed, kFloat };

absl::string_view IsacImplToString(IsacImpl impl) {
  switch (impl) {
    case IsacImpl::kFixed:
      return "fixed";
    case IsacImpl::kFloat:
      return "float";
  }
}

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

std::unique_ptr<AudioEncoder> CreateEncoder(IsacImpl impl,
                                            int sample_rate_hz,
                                            int frame_size_ms,
                                            int bitrate_bps) {
  RTC_CHECK(sample_rate_hz == 16000 || sample_rate_hz == 32000);
  RTC_CHECK(frame_size_ms == 30 || frame_size_ms == 60);
  RTC_CHECK_GT(bitrate_bps, 0);
  switch (impl) {
    case IsacImpl::kFixed: {
      AudioEncoderIsacFix::Config config;
      config.bit_rate = bitrate_bps;
      config.frame_size_ms = frame_size_ms;
      RTC_CHECK_EQ(16000, sample_rate_hz);
      return AudioEncoderIsacFix::MakeAudioEncoder(config, kPayloadType);
    }
    case IsacImpl::kFloat: {
      AudioEncoderIsacFloat::Config config;
      config.bit_rate = bitrate_bps;
      config.frame_size_ms = frame_size_ms;
      config.sample_rate_hz = sample_rate_hz;
      return AudioEncoderIsacFloat::MakeAudioEncoder(config, kPayloadType);
    }
  }
}

std::unique_ptr<AudioDecoder> CreateDecoder(IsacImpl impl, int sample_rate_hz) {
  RTC_CHECK(sample_rate_hz == 16000 || sample_rate_hz == 32000);
  switch (impl) {
    case IsacImpl::kFixed: {
      webrtc::AudioDecoderIsacFix::Config config;
      RTC_CHECK_EQ(16000, sample_rate_hz);
      return webrtc::AudioDecoderIsacFix::MakeAudioDecoder(config);
    }
    case IsacImpl::kFloat: {
      webrtc::AudioDecoderIsacFloat::Config config;
      config.sample_rate_hz = sample_rate_hz;
      return webrtc::AudioDecoderIsacFloat::MakeAudioDecoder(config);
    }
  }
}

struct EncoderTestParams {
  IsacImpl impl;
  int sample_rate_hz;
  int frame_size_ms;
};

class EncoderTest : public testing::TestWithParam<EncoderTestParams> {
 protected:
  EncoderTest() = default;
  IsacImpl GetIsacImpl() const { return GetParam().impl; }
  int GetSampleRateHz() const { return GetParam().sample_rate_hz; }
  int GetFrameSizeMs() const { return GetParam().frame_size_ms; }
};

TEST_P(EncoderTest, TestConfig) {
  for (int bitrate_bps : {10000, 21000, 32000}) {
    SCOPED_TRACE(bitrate_bps);
    auto encoder = CreateEncoder(GetIsacImpl(), GetSampleRateHz(),
                                 GetFrameSizeMs(), bitrate_bps);
    EXPECT_EQ(GetSampleRateHz(), encoder->SampleRateHz());
    EXPECT_EQ(size_t{1}, encoder->NumChannels());
    EXPECT_EQ(bitrate_bps, encoder->GetTargetBitrate());
  }
}

// Encodes an input audio sequence with a low and a high target bitrate and
// checks that the number of produces bytes in the first case is less than that
// of the second case.
TEST_P(EncoderTest, TestDifferentBitrates) {
  constexpr int kLowBps = 20000;
  constexpr int kHighBps = 25000;
  auto encoder_low = CreateEncoder(GetIsacImpl(), GetSampleRateHz(),
                                   GetFrameSizeMs(), kLowBps);
  auto encoder_high = CreateEncoder(GetIsacImpl(), GetSampleRateHz(),
                                    GetFrameSizeMs(), kHighBps);
  int num_bytes_low = 0;
  int num_bytes_high = 0;
  const auto in = GetRandomSamplesVector(
      /*size=*/rtc::CheckedDivExact(GetSampleRateHz(), 100));
  constexpr int kNumFrames = 12;
  for (int i = 0; i < kNumFrames; ++i) {
    rtc::Buffer low, high;
    encoder_low->Encode(/*rtp_timestamp=*/0, in, &low);
    encoder_high->Encode(/*rtp_timestamp=*/0, in, &high);
    num_bytes_low += low.size();
    num_bytes_high += high.size();
  }
  EXPECT_LT(num_bytes_low, num_bytes_high);
}

// Checks that the target and the measured bitrates are within tolerance.
// TODO(webrtc:11360): Add CBR flag to the config and re-enable test with CBR.
TEST_P(EncoderTest, DISABLED_TestBitrateNearTarget) {
  const auto in = GetRandomSamplesVector(
      /*size=*/rtc::CheckedDivExact(GetSampleRateHz(), 100));  // 10 ms.
  for (int bitrate_bps : {10000, 15000, 20000, 26000, 32000}) {
    SCOPED_TRACE(bitrate_bps);
    auto e = CreateEncoder(GetIsacImpl(), GetSampleRateHz(), GetFrameSizeMs(),
                           bitrate_bps);
    int num_bytes = 0;
    constexpr int kNumFrames = 60;
    for (int i = 0; i < kNumFrames; ++i) {
      rtc::Buffer encoded;
      e->Encode(/*rtp_timestamp=*/0, in, &encoded);
      num_bytes += encoded.size();
    }
    // Inverse of the duration of |kNumFrames| 10 ms frames (unit: seconds^-1).
    constexpr float kAudioDurationInv = 100.f / kNumFrames;
    const int measured_bitrate_bps = 8 * num_bytes * kAudioDurationInv;
    EXPECT_NEAR(bitrate_bps, measured_bitrate_bps, 1000);  // Max 1 kbps.
  }
}

// Creates tests for different encoder configurations and implementations.
INSTANTIATE_TEST_SUITE_P(
    IsacApiTest,
    EncoderTest,
    ::testing::ValuesIn([] {
      std::vector<EncoderTestParams> cases;
      for (IsacImpl impl : {IsacImpl::kFloat, IsacImpl::kFixed}) {
        for (int frame_size_ms : {30, 60}) {
          cases.push_back({impl, 16000, frame_size_ms});
        }
      }
      cases.push_back({IsacImpl::kFloat, 32000, 30});
      return cases;
    }()),
    [](const ::testing::TestParamInfo<EncoderTestParams>& info) {
      rtc::StringBuilder b;
      const auto& p = info.param;
      b << IsacImplToString(p.impl) << "_" << p.sample_rate_hz << "_"
        << p.frame_size_ms;
      return b.Release();
    });

struct DecoderTestParams {
  IsacImpl impl;
  int sample_rate_hz;
};

class DecoderTest : public testing::TestWithParam<DecoderTestParams> {
 protected:
  DecoderTest() = default;
  IsacImpl GetIsacImpl() const { return GetParam().impl; }
  int GetSampleRateHz() const { return GetParam().sample_rate_hz; }
};

TEST_P(DecoderTest, TestConfig) {
  auto decoder = CreateDecoder(GetIsacImpl(), GetSampleRateHz());
  EXPECT_EQ(GetSampleRateHz(), decoder->SampleRateHz());
  EXPECT_EQ(size_t{1}, decoder->Channels());
}

// Creates tests for different decoder configurations and implementations.
INSTANTIATE_TEST_SUITE_P(
    IsacApiTest,
    DecoderTest,
    ::testing::ValuesIn({DecoderTestParams{IsacImpl::kFixed, 16000},
                         DecoderTestParams{IsacImpl::kFloat, 16000},
                         DecoderTestParams{IsacImpl::kFloat, 32000}}),
    [](const ::testing::TestParamInfo<DecoderTestParams>& info) {
      const auto& p = info.param;
      return (rtc::StringBuilder()
              << IsacImplToString(p.impl) << "_" << p.sample_rate_hz)
          .Release();
    });

struct EncoderDecoderPairTestParams {
  int sample_rate_hz;
  int frame_size_ms;
  IsacImpl encoder_impl;
  IsacImpl decoder_impl;
};

class EncoderDecoderPairTest
    : public testing::TestWithParam<EncoderDecoderPairTestParams> {
 protected:
  EncoderDecoderPairTest()
      : input_frame_(GetRandomSamplesVector(GetInputFrameSize())) {}
  rtc::ArrayView<const int16_t> GetInputFrame() { return input_frame_; }
  int GetSampleRateHz() const { return GetParam().sample_rate_hz; }
  int GetEncoderFrameSizeMs() const { return GetParam().frame_size_ms; }
  IsacImpl GetEncoderIsacImpl() const { return GetParam().encoder_impl; }
  IsacImpl GetDecoderIsacImpl() const { return GetParam().decoder_impl; }

  int GetEncoderFrameSize() const {
    return GetEncoderFrameSizeMs() * GetSampleRateHz() / 1000;
  }

 private:
  const std::vector<int16_t> input_frame_;
  int GetInputFrameSize() const {
    return rtc::CheckedDivExact(GetParam().sample_rate_hz, 100);  // 10 ms.
  }
};

// Checks that the number of encoded and decoded samples match.
TEST_P(EncoderDecoderPairTest, EncodeDecode) {
  auto encoder = CreateEncoder(GetEncoderIsacImpl(), GetSampleRateHz(),
                               GetEncoderFrameSizeMs(), /*bitrate_bps=*/20000);
  auto decoder = CreateDecoder(GetDecoderIsacImpl(), GetSampleRateHz());
  const int encoder_frame_size = GetEncoderFrameSize();
  std::vector<int16_t> out(encoder_frame_size);
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

// Creates tests for different encoder frame sizes and different
// encoder/decoder implementations.
INSTANTIATE_TEST_SUITE_P(
    IsacApiTest,
    EncoderDecoderPairTest,
    ::testing::ValuesIn([] {
      std::vector<EncoderDecoderPairTestParams> cases;
      for (int frame_size_ms : {30, 60}) {
        for (IsacImpl enc : {IsacImpl::kFloat, IsacImpl::kFixed}) {
          for (IsacImpl dec : {IsacImpl::kFloat, IsacImpl::kFixed}) {
            cases.push_back({16000, frame_size_ms, enc, dec});
          }
        }
      }
      cases.push_back({32000, 30, IsacImpl::kFloat, IsacImpl::kFloat});
      return cases;
    }()),
    [](const ::testing::TestParamInfo<EncoderDecoderPairTestParams>& info) {
      rtc::StringBuilder b;
      const auto& p = info.param;
      b << p.sample_rate_hz << "_" << p.frame_size_ms << "_"
        << IsacImplToString(p.encoder_impl) << "_"
        << IsacImplToString(p.decoder_impl);
      return b.Release();
    });

}  // namespace
}  // namespace webrtc
