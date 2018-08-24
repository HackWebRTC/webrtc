/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>

#include <map>
#include <string>
#include <vector>

#include "rtc_tools/frame_analyzer/video_quality_analysis.h"
#include "rtc_tools/frame_analyzer/video_temporal_aligner.h"
#include "rtc_tools/simple_command_line_parser.h"
#include "rtc_tools/y4m_file_reader.h"
#include "test/testsupport/perf_test.h"

/*
 * A command line tool running PSNR and SSIM on a reference video and a test
 * video. The test video is a record of the reference video which can start at
 * an arbitrary point. It is possible that there will be repeated frames or
 * skipped frames as well. The video files should be 1420 Y4M videos.
 * The tool prints the result to standard output in the Chromium perf format:
 * RESULT <metric>:<label>= <values>
 *
 * The max value for PSNR is 48.0 (between equal frames), as for SSIM it is 1.0.
 *
 * Usage:
 * frame_analyzer --label=<test_label> --reference_file=<name_of_file>
 * --test_file_ref=<name_of_file>
 */
int main(int argc, char* argv[]) {
  std::string program_name = argv[0];
  std::string usage =
      "Compares the output video with the initially sent video."
      "\nExample usage:\n" +
      program_name +
      " --reference_file=ref.y4m --test_file=test.y4m\n"
      "Command line flags:\n"
      "  - label(string): The label to use for the perf output."
      " Default: MY_TEST\n"
      " Default: ref.y4m\n"
      "  - test_file(string): The test YUV file to run the analysis for."
      " Default: test_file.y4m\n"
      "  - chartjson_result_file: Where to store perf result in chartjson"
      " format. If not present, no perf result will be stored."
      " Default: None\n";

  webrtc::test::CommandLineParser parser;

  // Init the parser and set the usage message
  parser.Init(argc, argv);
  parser.SetUsageMessage(usage);

  parser.SetFlag("label", "MY_TEST");
  parser.SetFlag("reference_file", "ref.y4m");
  parser.SetFlag("test_file", "test.y4m");
  parser.SetFlag("chartjson_result_file", "");
  parser.SetFlag("help", "false");

  parser.ProcessFlags();
  if (parser.GetFlag("help") == "true") {
    parser.PrintUsageMessage();
    exit(EXIT_SUCCESS);
  }
  parser.PrintEnteredFlags();

  webrtc::test::ResultsContainer results;

  rtc::scoped_refptr<webrtc::test::Y4mFile> reference_video =
      webrtc::test::Y4mFile::Open(parser.GetFlag("reference_file"));
  rtc::scoped_refptr<webrtc::test::Y4mFile> test_video =
      webrtc::test::Y4mFile::Open(parser.GetFlag("test_file"));

  if (!reference_video || !test_video) {
    fprintf(stderr, "Error opening video files\n");
    return 0;
  }

  const std::vector<size_t> matching_indices =
      webrtc::test::FindMatchingFrameIndices(reference_video, test_video);

  results.frames =
      webrtc::test::RunAnalysis(reference_video, test_video, matching_indices);

  const std::vector<webrtc::test::Cluster> clusters =
      webrtc::test::CalculateFrameClusters(matching_indices);
  results.max_repeated_frames = webrtc::test::GetMaxRepeatedFrames(clusters);
  results.max_skipped_frames = webrtc::test::GetMaxSkippedFrames(clusters);
  results.total_skipped_frames =
      webrtc::test::GetTotalNumberOfSkippedFrames(clusters);
  results.decode_errors_ref = 0;
  results.decode_errors_test = 0;

  webrtc::test::PrintAnalysisResults(parser.GetFlag("label"), &results);

  std::string chartjson_result_file = parser.GetFlag("chartjson_result_file");
  if (!chartjson_result_file.empty()) {
    webrtc::test::WritePerfResults(chartjson_result_file);
  }

  return 0;
}
