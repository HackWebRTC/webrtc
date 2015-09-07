/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "gflags/gflags.h"
#include "webrtc/test/field_trial.h"
#include "webrtc/test/run_test.h"
#include "webrtc/video/full_stack.h"

namespace webrtc {
namespace flags {

DEFINE_string(title, "Full stack graph", "Graph title.");
std::string Title() {
  return static_cast<std::string>(FLAGS_title);
}

DEFINE_string(filename, "graph_data.txt", "Name of a target graph data file.");
std::string Filename() {
  return static_cast<std::string>(FLAGS_filename);
}

DEFINE_string(clip_name, "screenshare_slides", "Clip name, resource name.");
std::string ClipName() {
  return static_cast<std::string>(FLAGS_clip_name);
}

DEFINE_int32(width, 1850, "Video width (crops source).");
size_t Width() {
  return static_cast<size_t>(FLAGS_width);
}

DEFINE_int32(height, 1110, "Video height (crops source).");
size_t Height() {
  return static_cast<size_t>(FLAGS_height);
}

DEFINE_int32(fps, 5, "Frames per second.");
int Fps() {
  return static_cast<int>(FLAGS_fps);
}

DEFINE_int32(
    content_mode,
    1,
    "0 - real time video, 1 - screenshare static, 2 - screenshare scrolling.");
ContentMode ContentModeFlag() {
  switch (FLAGS_content_mode) {
    case 0:
      return ContentMode::kRealTimeVideo;
    case 1:
      return ContentMode::kScreensharingStaticImage;
    case 2:
      return ContentMode::kScreensharingScrollingImage;
    default:
      RTC_NOTREACHED() << "Unknown content mode!";
      return ContentMode::kScreensharingStaticImage;
  }
}

DEFINE_int32(test_duration, 60, "Duration of the test in seconds.");
int TestDuration() {
  return static_cast<int>(FLAGS_test_duration);
}

DEFINE_int32(min_bitrate, 50000, "Minimum video bitrate.");
int MinBitrate() {
  return static_cast<int>(FLAGS_min_bitrate);
}

DEFINE_int32(target_bitrate,
             500000,
             "Target video bitrate. (Default value here different than in full "
             "stack tests!)");
int TargetBitrate() {
  return static_cast<int>(FLAGS_target_bitrate);
}

DEFINE_int32(max_bitrate,
             500000,
             "Maximum video bitrate. (Default value here different than in "
             "full stack tests!)");
int MaxBitrate() {
  return static_cast<int>(FLAGS_max_bitrate);
}

DEFINE_string(codec, "VP9", "Video codec to use.");
std::string Codec() {
  return static_cast<std::string>(FLAGS_codec);
}

DEFINE_string(
    force_fieldtrials,
    "",
    "Field trials control experimental feature code which can be forced. "
    "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enable/"
    " will assign the group Enable to field trial WebRTC-FooFeature. Multiple "
    "trials are separated by \"/\"");
}  // namespace flags

class FullStackGenGraph : public FullStackTest {
 public:
  void TestBody() override {
    std::string title = flags::Title();
    std::string clip_name = flags::ClipName();
    FullStackTestParams params = {
        title.c_str(),
        {clip_name.c_str(), flags::Width(), flags::Height(), flags::Fps()},
        flags::ContentModeFlag(),
        flags::MinBitrate(),
        flags::TargetBitrate(),
        flags::MaxBitrate(),
        0.0,  // avg_psnr_threshold
        0.0,  // avg_ssim_threshold
        flags::TestDuration(),
        flags::Codec()};
    params.graph_data_output_filename = flags::Filename();

    RunTest(params);
  }
};

void FullStackRun(void) {
  FullStackGenGraph full_stack;
  full_stack.TestBody();
}
}  // namespace webrtc

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  google::ParseCommandLineFlags(&argc, &argv, true);
  webrtc::test::InitFieldTrialsFromString(
      webrtc::flags::FLAGS_force_fieldtrials);
  webrtc::test::RunTest(webrtc::FullStackRun);
  return 0;
}
