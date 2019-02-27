/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_ANALYZER_AUDIO_DEFAULT_AUDIO_QUALITY_ANALYZER_H_
#define TEST_PC_E2E_ANALYZER_AUDIO_DEFAULT_AUDIO_QUALITY_ANALYZER_H_

#include "absl/strings/string_view.h"
#include "api/stats_types.h"
#include "test/pc/e2e/api/audio_quality_analyzer_interface.h"

namespace webrtc {
namespace test {

class DefaultAudioQualityAnalyzer : public AudioQualityAnalyzerInterface {
 public:
  void Start(std::string test_case_name) override;
  void OnStatsReports(absl::string_view pc_label,
                      const StatsReports& stats_reports) override;

 private:
  std::string test_case_name_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_AUDIO_DEFAULT_AUDIO_QUALITY_ANALYZER_H_
