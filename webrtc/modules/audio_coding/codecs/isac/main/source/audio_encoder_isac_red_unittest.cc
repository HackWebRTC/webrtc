/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/audio_coding/codecs/isac/main/interface/audio_encoder_isac.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

// Simply check that AudioEncoderDecoderIsacRed produces more encoded bytes
// than AudioEncoderDecoderIsac. Also check that the redundancy information is
// populated in the EncodedInfo.
TEST(AudioEncoderIsacRedTest, CompareRedAndNoRed) {
  static const int kSampleRateHz = 16000;
  static const int k10MsSamples = kSampleRateHz / 100;
  static const int kRedPayloadType = 100;
  // Fill the input array with pseudo-random noise in the range [-1000, 1000].
  int16_t input[k10MsSamples];
  srand(1418811752);
  for (int i = 0; i < k10MsSamples; ++i) {
    double r = rand();  // NOLINT(runtime/threadsafe_fn)
    input[i] = (r / RAND_MAX) * 2000 - 1000;
  }
  static const size_t kMaxEncodedSizeBytes = 1000;
  uint8_t encoded[kMaxEncodedSizeBytes];
  uint8_t red_encoded[kMaxEncodedSizeBytes];
  AudioEncoderDecoderIsac::Config config;
  config.sample_rate_hz = kSampleRateHz;
  AudioEncoderDecoderIsac isac_encoder(config);
  AudioEncoderDecoderIsacRed::Config red_config;
  red_config.sample_rate_hz = kSampleRateHz;
  red_config.red_payload_type = kRedPayloadType;
  ASSERT_NE(red_config.red_payload_type, red_config.payload_type)
      << "iSAC and RED payload types must be different.";
  AudioEncoderDecoderIsacRed isac_red_encoder(red_config);
  AudioEncoder::EncodedInfo info, red_info;

  // Note that we are not expecting any output from the redundant encoder until
  // the 6th block of 10 ms has been processed. This is because in RED mode,
  // iSAC will not output the first 30 ms frame.
  for (int i = 0; i < 6; ++i) {
    EXPECT_EQ(0u, red_info.encoded_bytes);
    EXPECT_EQ(0u, red_info.redundant.size());
    const uint32_t timestamp = static_cast<uint32_t>(i);
    EXPECT_TRUE(isac_encoder.Encode(timestamp, input, k10MsSamples,
                                    kMaxEncodedSizeBytes, encoded, &info));
    EXPECT_TRUE(isac_red_encoder.Encode(timestamp, input, k10MsSamples,
                                        kMaxEncodedSizeBytes, red_encoded,
                                        &red_info));
  }
  EXPECT_GT(info.encoded_bytes, 0u)
      << "Regular codec did not produce any output";
  EXPECT_GT(red_info.encoded_bytes, info.encoded_bytes)
      << "Redundant payload seems to be missing";
  ASSERT_EQ(2u, red_info.redundant.size()) << "Redundancy vector not populated";
  ASSERT_EQ(info.encoded_bytes, red_info.redundant[0].encoded_bytes)
      << "Primary payload should be same length as non-redundant payload";
  // Check that |encoded| and the primary part of |red_encoded| are identical.
  EXPECT_EQ(0, memcmp(encoded, red_encoded, info.encoded_bytes));
  EXPECT_GT(red_info.redundant[0].encoded_bytes,
            red_info.redundant[1].encoded_bytes)
      << "Redundant payload should be smaller than primary";
  EXPECT_EQ(red_info.encoded_bytes, red_info.redundant[0].encoded_bytes +
                                        red_info.redundant[1].encoded_bytes)
      << "Encoded sizes don't add up";
  EXPECT_EQ(3u, red_info.redundant[0].encoded_timestamp)
      << "Primary timestamp is wrong";
  EXPECT_EQ(0u, red_info.redundant[1].encoded_timestamp)
      << "Secondary timestamp is wrong";
}
}  // namespace webrtc
