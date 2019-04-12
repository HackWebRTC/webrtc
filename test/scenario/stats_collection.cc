/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/scenario/stats_collection.h"

#include <utility>

#include "common_video/libyuv/include/webrtc_libyuv.h"

namespace webrtc {
namespace test {

VideoQualityAnalyzer::VideoQualityAnalyzer(
    VideoQualityAnalyzerConfig config,
    std::unique_ptr<RtcEventLogOutput> writer)
    : config_(config), writer_(std::move(writer)) {
  if (writer_) {
    PrintHeaders();
  }
}

VideoQualityAnalyzer::~VideoQualityAnalyzer() = default;

void VideoQualityAnalyzer::PrintHeaders() {
  writer_->Write(
      "capture_time render_time capture_width capture_height render_width "
      "render_height psnr\n");
}

std::function<void(const VideoFramePair&)> VideoQualityAnalyzer::Handler() {
  return [this](VideoFramePair pair) { HandleFramePair(pair); };
}

void VideoQualityAnalyzer::HandleFramePair(VideoFramePair sample) {
  double psnr = NAN;
  RTC_CHECK(sample.captured);
  ++stats_.captures_count;
  if (!sample.decoded) {
    ++stats_.lost_count;
  } else {
    psnr = I420PSNR(*sample.captured->ToI420(), *sample.decoded->ToI420());
    ++stats_.valid_count;
    stats_.end_to_end_seconds.AddSample(
        (sample.render_time - sample.capture_time).seconds<double>());
    stats_.psnr.AddSample(psnr);
  }
  if (writer_) {
    LogWriteFormat(writer_.get(), "%.3f %.3f %.3f %i %i %i %i %.3f\n",
                   sample.capture_time.seconds<double>(),
                   sample.render_time.seconds<double>(),
                   sample.captured->width(), sample.captured->height(),
                   sample.decoded->width(), sample.decoded->height(), psnr);
  }
}

VideoQualityStats VideoQualityAnalyzer::stats() const {
  return stats_;
}

}  // namespace test
}  // namespace webrtc
