/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/common_audio/wav_header.h"
#include "webrtc/system_wrappers/interface/compile_assert.h"

// Try various choices of WAV header parameters, and make sure that the good
// ones are accepted and the bad ones rejected.
TEST(WavHeaderTest, CheckWavParameters) {
  // Try some really stupid values for one parameter at a time.
  EXPECT_TRUE(webrtc::CheckWavParameters(1, 8000, webrtc::kWavFormatPcm, 1, 0));
  EXPECT_FALSE(
      webrtc::CheckWavParameters(0, 8000, webrtc::kWavFormatPcm, 1, 0));
  EXPECT_FALSE(
      webrtc::CheckWavParameters(-1, 8000, webrtc::kWavFormatPcm, 1, 0));
  EXPECT_FALSE(webrtc::CheckWavParameters(1, 0, webrtc::kWavFormatPcm, 1, 0));
  EXPECT_FALSE(webrtc::CheckWavParameters(1, 8000, webrtc::WavFormat(0), 1, 0));
  EXPECT_FALSE(
      webrtc::CheckWavParameters(1, 8000, webrtc::kWavFormatPcm, 0, 0));

  // Try invalid format/bytes-per-sample combinations.
  EXPECT_TRUE(webrtc::CheckWavParameters(1, 8000, webrtc::kWavFormatPcm, 2, 0));
  EXPECT_FALSE(
      webrtc::CheckWavParameters(1, 8000, webrtc::kWavFormatPcm, 4, 0));
  EXPECT_FALSE(
      webrtc::CheckWavParameters(1, 8000, webrtc::kWavFormatALaw, 2, 0));
  EXPECT_FALSE(
      webrtc::CheckWavParameters(1, 8000, webrtc::kWavFormatMuLaw, 2, 0));

  // Too large values.
  EXPECT_FALSE(webrtc::CheckWavParameters(
      1 << 20, 1 << 20, webrtc::kWavFormatPcm, 1, 0));
  EXPECT_FALSE(webrtc::CheckWavParameters(
      1, 8000, webrtc::kWavFormatPcm, 1, std::numeric_limits<uint32_t>::max()));

  // Not the same number of samples for each channel.
  EXPECT_FALSE(
      webrtc::CheckWavParameters(3, 8000, webrtc::kWavFormatPcm, 1, 5));
}

// Try writing a WAV header and make sure it looks OK.
TEST(WavHeaderTest, WriteWavHeader) {
  static const int kSize = 4 + webrtc::kWavHeaderSize + 4;
  uint8_t buf[kSize];
  memset(buf, 0xa4, sizeof(buf));
  webrtc::WriteWavHeader(
      buf + 4, 17, 12345, webrtc::kWavFormatALaw, 1, 123457689);
  static const uint8_t kExpectedBuf[] = {
    0xa4, 0xa4, 0xa4, 0xa4,  // untouched bytes before header
    'R', 'I', 'F', 'F',
    0xbd, 0xd0, 0x5b, 0x07,  // size of whole file - 8: 123457689 + 44 - 8
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    16, 0, 0, 0,  // size of fmt block - 8: 24 - 8
    6, 0,  // format: A-law (6)
    17, 0,  // channels: 17
    0x39, 0x30, 0, 0,  // sample rate: 12345
    0xc9, 0x33, 0x03, 0,  // byte rate: 1 * 17 * 12345
    17, 0,  // block align: NumChannels * BytesPerSample
    8, 0,  // bits per sample: 1 * 8
    'd', 'a', 't', 'a',
    0x99, 0xd0, 0x5b, 0x07,  // size of payload: 123457689
    0xa4, 0xa4, 0xa4, 0xa4,  // untouched bytes after header
  };
  COMPILE_ASSERT(sizeof(kExpectedBuf) == kSize, buf_size);
  EXPECT_EQ(0, memcmp(kExpectedBuf, buf, kSize));
}
