/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>

#include "google/gflags.h"

#include "testsupport/frame_analyzer/video_quality_analysis.h"

#define STATS_LINE_LENGTH 25

DEFINE_string(stats_file, "stats.txt", "The full name of the file "
              "containing the stats after decoding of the received YUV video");
DEFINE_string(reference_file, "ref.yuv", "The reference YUV file to compare "
              "against.");
DEFINE_string(test_file, "test.yuv", "The test YUV file to run the analysis "
              "for.");
DEFINE_int32(width, 352, "The width of the refence and test files.");
DEFINE_int32(height, 288, "The height of the reference and test files.");


int main(int argc, char** argv) {
  std::string program_name = argv[0];
  std::string usage = "Compares the output video with the initially sent video."
      "\nRun " + program_name + " --helpshort for usage.\n"
      "Example usage:\n" + program_name + " --stats_file=stats.txt "
      "--reference_file=ref.yuv --test_file=test.yuv --width=352 --height=288";
  google::SetUsageMessage(usage);

  google::ParseCommandLineFlags(&argc, &argv, true);

  fprintf(stdout, "You have entered:\n");
  fprintf(stdout, "stats_file=%s, reference_file=%s, test_file=%s, width=%d, "
          "height=%d\n", FLAGS_stats_file.c_str(), FLAGS_reference_file.c_str(),
          FLAGS_test_file.c_str(), FLAGS_width, FLAGS_height);

  webrtc::test::ResultsContainer results;

  webrtc::test::RunAnalysis(FLAGS_reference_file.c_str(),
                            FLAGS_test_file.c_str(), FLAGS_stats_file.c_str(),
                            FLAGS_width, FLAGS_height, &results);

  webrtc::test::PrintAnalysisResults(&results);
  webrtc::test::PrintMaxRepeatedAndSkippedFrames(FLAGS_stats_file.c_str());
}
