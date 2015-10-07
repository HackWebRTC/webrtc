/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_VIDEO_VIDEO_QUALITY_TEST_H_
#define WEBRTC_VIDEO_VIDEO_QUALITY_TEST_H_

#include <string>

#include "webrtc/test/call_test.h"
#include "webrtc/test/frame_generator.h"
#include "webrtc/test/testsupport/trace_to_stderr.h"

namespace webrtc {

class VideoQualityTest : public test::CallTest {
 public:
  // Parameters are grouped into smaller structs to make it easier to set
  // the desired elements and skip unused, using aggregate initialization.
  // Unfortunately, C++11 (as opposed to C11) doesn't support unnamed structs,
  // which makes the implementation of VideoQualityTest a bit uglier.
  struct Params {
    struct {
      size_t width;
      size_t height;
      int32_t fps;
      int min_bitrate_bps;
      int target_bitrate_bps;
      int max_bitrate_bps;
      std::string codec;
      size_t num_temporal_layers;
      int min_transmit_bps;

      Call::Config::BitrateConfig call_bitrate_config;
      size_t tl_discard_threshold;
      bool send_side_bwe;
    } common;
    struct {  // Video-specific settings.
      std::string clip_name;
    } video;
    struct {  // Screenshare-specific settings.
      bool enabled;
      int32_t slide_change_interval;
      int32_t scroll_duration;
    } screenshare;
    struct {  // Analyzer settings.
      std::string test_label;
      double avg_psnr_threshold;
      double avg_ssim_threshold;
      int test_durations_secs;
      std::string graph_data_output_filename;
    } analyzer;
    FakeNetworkPipe::Config pipe;
    bool logs;
  };

  VideoQualityTest();
  void RunWithAnalyzer(const Params& params);
  void RunWithVideoRenderer(const Params& params);

 protected:
  // No-op implementation to be able to instantiate this class from non-TEST_F
  // locations.
  void TestBody() override;

  void CreateCapturer(const Params& params, VideoCaptureInput* input);
  void ValidateParams(const Params& params);
  void SetupFullStack(const Params& params,
                      Transport* send_transport,
                      Transport* recv_transport);
  void SetupScreenshare(const Params& params);

  // We need a more general capturer than the FrameGeneratorCapturer.
  rtc::scoped_ptr<test::VideoCapturer> capturer_;
  rtc::scoped_ptr<test::TraceToStderr> trace_to_stderr_;
  rtc::scoped_ptr<test::FrameGenerator> frame_generator_;
  rtc::scoped_ptr<VideoEncoder> encoder_;
  VideoCodecUnion codec_settings_;
  Clock* const clock_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_VIDEO_QUALITY_TEST_H_
