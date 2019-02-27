/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/audio/default_audio_quality_analyzer.h"

#include "rtc_base/logging.h"

namespace webrtc {
namespace test {

void DefaultAudioQualityAnalyzer::Start(std::string test_case_name) {
  test_case_name_ = std::move(test_case_name);
}

void DefaultAudioQualityAnalyzer::OnStatsReports(
    absl::string_view pc_label,
    const StatsReports& stats_reports) {
  // TODO(bugs.webrtc.org/10138): Implement audio stats collection.
}

}  // namespace test
}  // namespace webrtc
