/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <numeric>
#include <sstream>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/buffer.h"
#include "webrtc/modules/audio_coding/codecs/isac/fix/interface/audio_encoder_isacfix.h"
#include "webrtc/modules/audio_coding/codecs/isac/main/interface/audio_encoder_isac.h"
#include "webrtc/modules/audio_coding/neteq/tools/input_audio_file.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {

namespace {

std::vector<int16_t> LoadSpeechData() {
  webrtc::test::InputAudioFile input_file(
      webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm"));
  static const int kIsacNumberOfSamples = 32 * 60;  // 60 ms at 32 kHz
  std::vector<int16_t> speech_data(kIsacNumberOfSamples);
  input_file.Read(kIsacNumberOfSamples, speech_data.data());
  return speech_data;
}

template <typename T>
IsacBandwidthInfo GetBwInfo(typename T::instance_type* inst) {
  IsacBandwidthInfo bi;
  T::GetBandwidthInfo(inst, &bi);
  EXPECT_TRUE(bi.in_use);
  return bi;
}

template <typename T>
rtc::Buffer EncodePacket(typename T::instance_type* inst,
                         const IsacBandwidthInfo* bi,
                         const int16_t* speech_data,
                         int framesize_ms) {
  rtc::Buffer output(1000);
  for (int i = 0;; ++i) {
    if (bi)
      T::SetBandwidthInfo(inst, bi);
    int encoded_bytes = T::Encode(inst, speech_data, output.data());
    if (i + 1 == framesize_ms / 10) {
      EXPECT_GT(encoded_bytes, 0);
      EXPECT_LE(static_cast<size_t>(encoded_bytes), output.size());
      output.SetSize(encoded_bytes);
      return output;
    }
    EXPECT_EQ(0, encoded_bytes);
  }
}

class BoundedCapacityChannel final {
 public:
  BoundedCapacityChannel(int rate_bits_per_second)
      : current_time_rtp_(0),
        channel_rate_bytes_per_sample_(rate_bits_per_second /
                                       (8.0 * kSamplesPerSecond)) {}

  // Simulate sending the given number of bytes at the given RTP time. Returns
  // the new current RTP time after the sending is done.
  int Send(int send_time_rtp, int nbytes) {
    current_time_rtp_ = std::max(current_time_rtp_, send_time_rtp) +
                        nbytes / channel_rate_bytes_per_sample_;
    return current_time_rtp_;
  }

 private:
  int current_time_rtp_;
  // The somewhat strange unit for channel rate, bytes per sample, is because
  // RTP time is measured in samples:
  const double channel_rate_bytes_per_sample_;
  static const int kSamplesPerSecond = 16000;
};

template <typename T, bool adaptive>
struct TestParam {};

template <>
struct TestParam<IsacFloat, true> {
  static const int time_to_settle = 200;
  static int ExpectedRateBitsPerSecond(int rate_bits_per_second) {
    return rate_bits_per_second;
  }
};

template <>
struct TestParam<IsacFix, true> {
  static const int time_to_settle = 350;
  static int ExpectedRateBitsPerSecond(int rate_bits_per_second) {
    // For some reason, IsacFix fails to adapt to the channel's actual
    // bandwidth. Instead, it settles on a few hundred packets at 10kbit/s,
    // then a few hundred at 5kbit/s, then a few hundred at 10kbit/s, and so
    // on. The 200 packets starting at 350 are in the middle of the first
    // 10kbit/s run.
    return 10000;
  }
};

template <>
struct TestParam<IsacFloat, false> {
  static const int time_to_settle = 0;
  static int ExpectedRateBitsPerSecond(int rate_bits_per_second) {
    return 32000;
  }
};

template <>
struct TestParam<IsacFix, false> {
  static const int time_to_settle = 0;
  static int ExpectedRateBitsPerSecond(int rate_bits_per_second) {
    return 16000;
  }
};

// Test that the iSAC encoder produces identical output whether or not we use a
// conjoined encoder+decoder pair or a separate encoder and decoder that
// communicate BW estimation info explicitly.
template <typename T, bool adaptive>
void TestGetSetBandwidthInfo(const int16_t* speech_data,
                             int rate_bits_per_second) {
  using Param = TestParam<T, adaptive>;
  const int framesize_ms = adaptive ? 60 : 30;

  // Conjoined encoder/decoder pair:
  typename T::instance_type* encdec;
  ASSERT_EQ(0, T::Create(&encdec));
  ASSERT_EQ(0, T::EncoderInit(encdec, adaptive ? 0 : 1));
  ASSERT_EQ(0, T::DecoderInit(encdec));

  // Disjoint encoder/decoder pair:
  typename T::instance_type* enc;
  ASSERT_EQ(0, T::Create(&enc));
  ASSERT_EQ(0, T::EncoderInit(enc, adaptive ? 0 : 1));
  typename T::instance_type* dec;
  ASSERT_EQ(0, T::Create(&dec));
  ASSERT_EQ(0, T::DecoderInit(dec));

  // 0. Get initial BW info from decoder.
  auto bi = GetBwInfo<T>(dec);

  BoundedCapacityChannel channel1(rate_bits_per_second),
      channel2(rate_bits_per_second);
  std::vector<size_t> packet_sizes;
  for (int i = 0; i < Param::time_to_settle + 200; ++i) {
    std::ostringstream ss;
    ss << " i = " << i;
    SCOPED_TRACE(ss.str());

    // 1. Encode 6 * 10 ms (adaptive) or 3 * 10 ms (nonadaptive). The separate
    // encoder is given the BW info before each encode call.
    auto bitstream1 =
        EncodePacket<T>(encdec, nullptr, speech_data, framesize_ms);
    auto bitstream2 = EncodePacket<T>(enc, &bi, speech_data, framesize_ms);
    EXPECT_EQ(bitstream1, bitstream2);
    if (i > Param::time_to_settle)
      packet_sizes.push_back(bitstream1.size());

    // 2. Deliver the encoded data to the decoders (but don't actually ask them
    // to decode it; that's not necessary). Then get new BW info from the
    // separate decoder.
    const int samples_per_packet = 16 * framesize_ms;
    const int send_time = i * samples_per_packet;
    EXPECT_EQ(0, T::UpdateBwEstimate(
                     encdec, bitstream1.data(), bitstream1.size(), i, send_time,
                     channel1.Send(send_time, bitstream1.size())));
    EXPECT_EQ(0, T::UpdateBwEstimate(
                     dec, bitstream2.data(), bitstream2.size(), i, send_time,
                     channel2.Send(send_time, bitstream2.size())));
    bi = GetBwInfo<T>(dec);
  }

  EXPECT_EQ(0, T::Free(encdec));
  EXPECT_EQ(0, T::Free(enc));
  EXPECT_EQ(0, T::Free(dec));

  // The average send bitrate is close to the channel's capacity.
  double avg_size =
      std::accumulate(packet_sizes.begin(), packet_sizes.end(), 0) /
      static_cast<double>(packet_sizes.size());
  double avg_rate_bits_per_second = 8.0 * avg_size / (framesize_ms * 1e-3);
  double expected_rate_bits_per_second =
      Param::ExpectedRateBitsPerSecond(rate_bits_per_second);
  EXPECT_GT(avg_rate_bits_per_second / expected_rate_bits_per_second, 0.95);
  EXPECT_LT(avg_rate_bits_per_second / expected_rate_bits_per_second, 1.06);

  // The largest packet isn't that large, and the smallest not that small.
  size_t min_size = *std::min_element(packet_sizes.begin(), packet_sizes.end());
  size_t max_size = *std::max_element(packet_sizes.begin(), packet_sizes.end());
  double size_range = max_size - min_size;
  EXPECT_LE(size_range / avg_size, 0.16);
}

}  // namespace

TEST(IsacCommonTest, GetSetBandwidthInfoFloat12kAdaptive) {
  TestGetSetBandwidthInfo<IsacFloat, true>(LoadSpeechData().data(), 12000);
}

TEST(IsacCommonTest, GetSetBandwidthInfoFloat15kAdaptive) {
  TestGetSetBandwidthInfo<IsacFloat, true>(LoadSpeechData().data(), 15000);
}

TEST(IsacCommonTest, GetSetBandwidthInfoFloat19kAdaptive) {
  TestGetSetBandwidthInfo<IsacFloat, true>(LoadSpeechData().data(), 19000);
}

TEST(IsacCommonTest, GetSetBandwidthInfoFloat22kAdaptive) {
  TestGetSetBandwidthInfo<IsacFloat, true>(LoadSpeechData().data(), 22000);
}

TEST(IsacCommonTest, GetSetBandwidthInfoFix12kAdaptive) {
  TestGetSetBandwidthInfo<IsacFix, true>(LoadSpeechData().data(), 12000);
}

TEST(IsacCommonTest, GetSetBandwidthInfoFix15kAdaptive) {
  TestGetSetBandwidthInfo<IsacFix, true>(LoadSpeechData().data(), 15000);
}

TEST(IsacCommonTest, GetSetBandwidthInfoFix19kAdaptive) {
  TestGetSetBandwidthInfo<IsacFix, true>(LoadSpeechData().data(), 19000);
}

TEST(IsacCommonTest, GetSetBandwidthInfoFix22kAdaptive) {
  TestGetSetBandwidthInfo<IsacFix, true>(LoadSpeechData().data(), 22000);
}

TEST(IsacCommonTest, GetSetBandwidthInfoFloat12k) {
  TestGetSetBandwidthInfo<IsacFloat, false>(LoadSpeechData().data(), 12000);
}

TEST(IsacCommonTest, GetSetBandwidthInfoFloat15k) {
  TestGetSetBandwidthInfo<IsacFloat, false>(LoadSpeechData().data(), 15000);
}

TEST(IsacCommonTest, GetSetBandwidthInfoFloat19k) {
  TestGetSetBandwidthInfo<IsacFloat, false>(LoadSpeechData().data(), 19000);
}

TEST(IsacCommonTest, GetSetBandwidthInfoFloat22k) {
  TestGetSetBandwidthInfo<IsacFloat, false>(LoadSpeechData().data(), 22000);
}

TEST(IsacCommonTest, GetSetBandwidthInfoFix12k) {
  TestGetSetBandwidthInfo<IsacFix, false>(LoadSpeechData().data(), 12000);
}

TEST(IsacCommonTest, GetSetBandwidthInfoFix15k) {
  TestGetSetBandwidthInfo<IsacFix, false>(LoadSpeechData().data(), 15000);
}

TEST(IsacCommonTest, GetSetBandwidthInfoFix19k) {
  TestGetSetBandwidthInfo<IsacFix, false>(LoadSpeechData().data(), 19000);
}

TEST(IsacCommonTest, GetSetBandwidthInfoFix22k) {
  TestGetSetBandwidthInfo<IsacFix, false>(LoadSpeechData().data(), 22000);
}

}  // namespace webrtc
