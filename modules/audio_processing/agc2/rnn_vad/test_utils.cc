/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/test_utils.h"

#include "rtc_base/checks.h"
#include "rtc_base/ptr_util.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace rnn_vad {
namespace test {
namespace {

using ReaderPairType =
    std::pair<std::unique_ptr<BinaryFileReader<float>>, const size_t>;

}  // namespace

using webrtc::test::ResourcePath;

void ExpectNearAbsolute(rtc::ArrayView<const float> expected,
                        rtc::ArrayView<const float> computed,
                        float tolerance) {
  ASSERT_EQ(expected.size(), computed.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_NEAR(expected[i], computed[i], tolerance);
  }
}

ReaderPairType CreatePitchBuffer24kHzReader() {
  auto ptr = rtc::MakeUnique<BinaryFileReader<float>>(
      ResourcePath("audio_processing/agc2/rnn_vad/pitch_buf_24k", "dat"), 864);
  return {std::move(ptr),
          rtc::CheckedDivExact(ptr->data_length(), static_cast<size_t>(864))};
}

ReaderPairType CreateLpResidualAndPitchPeriodGainReader() {
  constexpr size_t num_lp_residual_coeffs = 864;
  auto ptr = rtc::MakeUnique<BinaryFileReader<float>>(
      ResourcePath("audio_processing/agc2/rnn_vad/pitch_lp_res", "dat"),
      num_lp_residual_coeffs);
  return {std::move(ptr),
          rtc::CheckedDivExact(ptr->data_length(), 2 + num_lp_residual_coeffs)};
}

}  // namespace test
}  // namespace rnn_vad
}  // namespace webrtc
