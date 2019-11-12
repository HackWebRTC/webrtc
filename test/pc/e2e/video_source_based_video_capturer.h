/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_VIDEO_SOURCE_BASED_VIDEO_CAPTURER_H_
#define TEST_PC_E2E_VIDEO_SOURCE_BASED_VIDEO_CAPTURER_H_

#include <memory>

#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "test/test_video_capturer.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// Used to forward VideoFrame's provided by custom video source into video
// quality analyzer and VideoAdapter inside TestVideoCapturer and then properly
// broadcast them.
class VideoSourceBasedVideoCapturer
    : public webrtc::test::TestVideoCapturer,
      public rtc::VideoSinkInterface<VideoFrame> {
 public:
  VideoSourceBasedVideoCapturer(
      std::unique_ptr<rtc::VideoSourceInterface<VideoFrame>> source);
  ~VideoSourceBasedVideoCapturer() override;

  void OnFrame(const VideoFrame& frame) override;

 private:
  std::unique_ptr<rtc::VideoSourceInterface<VideoFrame>> source_;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_VIDEO_SOURCE_BASED_VIDEO_CAPTURER_H_
