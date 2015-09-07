/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_VIDEO_FULL_STACK_H_
#define WEBRTC_VIDEO_FULL_STACK_H_

#include <string>

#include "webrtc/test/call_test.h"
#include "webrtc/test/direct_transport.h"

namespace webrtc {

enum class ContentMode {
  kRealTimeVideo,
  kScreensharingStaticImage,
  kScreensharingScrollingImage,
};

struct FullStackTestParams {
  const char* test_label;
  struct {
    const char* name;
    size_t width, height;
    int fps;
  } clip;
  ContentMode mode;
  int min_bitrate_bps;
  int target_bitrate_bps;
  int max_bitrate_bps;
  double avg_psnr_threshold;
  double avg_ssim_threshold;
  int test_durations_secs;
  std::string codec;
  FakeNetworkPipe::Config link;
  std::string graph_data_output_filename;
};

class FullStackTest : public test::CallTest {
 protected:
  void RunTest(const FullStackTestParams& params);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_FULL_STACK_H_
