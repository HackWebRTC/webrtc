/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/video_source_based_video_capturer.h"

#include <utility>

namespace webrtc {
namespace webrtc_pc_e2e {

VideoSourceBasedVideoCapturer::VideoSourceBasedVideoCapturer(
    std::unique_ptr<rtc::VideoSourceInterface<VideoFrame>> source)
    : source_(std::move(source)) {
  source_->AddOrUpdateSink(this, rtc::VideoSinkWants());
}
VideoSourceBasedVideoCapturer::~VideoSourceBasedVideoCapturer() {
  source_->RemoveSink(this);
}

void VideoSourceBasedVideoCapturer::OnFrame(const VideoFrame& frame) {
  TestVideoCapturer::OnFrame(frame);
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
