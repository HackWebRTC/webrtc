/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <stdio.h>
#include <stdlib.h>

#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "rtc_tools/frame_analyzer/reference_less_video_analysis_lib.h"

ABSL_FLAG(std::string,
          video_file,
          "",
          "Path of the video file to be analyzed, only y4m file format is "
          "supported");

int main(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);
  // TODO(bugs.webrtc.org/10616): Add program usage message when Abseil
  // flags supports it.
  // This captures the freezing metrics for reference less video analysis.
  // std::string usage =
  //     "Outputs the freezing score by comparing current frame "
  //     "with the previous frame.\nExample usage:\n" +
  //     program_name +
  //     " --video_file=video_file.y4m\n"
  //     "Command line flags:\n"
  //     "  - video_file(string): Path of the video "
  //     "file to be analyzed. Only y4m file format is supported.\n";

  std::string video_file = absl::GetFlag(FLAGS_video_file);
  if (video_file.empty()) {
    exit(EXIT_FAILURE);
  }

  return run_analysis(video_file);
}
