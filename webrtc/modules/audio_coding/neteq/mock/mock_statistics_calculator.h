/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_STATISTICS_CALCULATOR_H_
#define WEBRTC_MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_STATISTICS_CALCULATOR_H_

#include "webrtc/modules/audio_coding/neteq/statistics_calculator.h"

#include "webrtc/test/gmock.h"

namespace webrtc {

class MockStatisticsCalculator : public StatisticsCalculator {
 public:
  // For current unittest, we mock only one method.
  MOCK_METHOD1(PacketsDiscarded, void(size_t num_packets));
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_STATISTICS_CALCULATOR_H_
