/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_SCENARIO_STATS_COLLECTION_H_
#define TEST_SCENARIO_STATS_COLLECTION_H_

#include <memory>

#include "test/logging/log_writer.h"
#include "test/scenario/performance_stats.h"

namespace webrtc {
namespace test {

struct VideoQualityAnalyzerConfig {
  double psnr_coverage = 1;
};

class VideoQualityAnalyzer {
 public:
  explicit VideoQualityAnalyzer(
      VideoQualityAnalyzerConfig config = VideoQualityAnalyzerConfig(),
      std::unique_ptr<RtcEventLogOutput> writer = nullptr);
  ~VideoQualityAnalyzer();
  void HandleFramePair(VideoFramePair sample);
  VideoQualityStats stats() const;
  void PrintHeaders();
  void PrintFrameInfo(const VideoFramePair& sample);
  std::function<void(const VideoFramePair&)> Handler();

 private:
  const VideoQualityAnalyzerConfig config_;
  VideoQualityStats stats_;
  const std::unique_ptr<RtcEventLogOutput> writer_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_STATS_COLLECTION_H_
