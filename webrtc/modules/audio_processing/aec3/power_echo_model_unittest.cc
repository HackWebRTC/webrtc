/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/power_echo_model.h"

#include <array>
#include <string>
#include <vector>

#include "webrtc/base/random.h"
#include "webrtc/modules/audio_processing/aec3/aec_state.h"
#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/modules/audio_processing/aec3/aec3_fft.h"
#include "webrtc/modules/audio_processing/aec3/echo_path_variability.h"
#include "webrtc/modules/audio_processing/test/echo_canceller_test_tools.h"

#include "webrtc/test/gtest.h"

namespace webrtc {

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies that the check for non-null output parameter works.
TEST(PowerEchoModel, NullEstimateEchoOutput) {
  PowerEchoModel model;
  std::array<float, kFftLengthBy2Plus1> Y2;
  AecState aec_state;
  RenderBuffer X_buffer(Aec3Optimization::kNone, 3,
                        model.MinFarendBufferLength(),
                        std::vector<size_t>(1, model.MinFarendBufferLength()));

  EXPECT_DEATH(model.EstimateEcho(X_buffer, Y2, aec_state, nullptr), "");
}

#endif


}  // namespace webrtc
