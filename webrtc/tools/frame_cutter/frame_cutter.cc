/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/tools/frame_cutter/frame_cutter_lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webrtc/tools/simple_command_line_parser.h"

// A command-line tool to edit a YUV-video (I420 sub-sampled).
int main(int argc, char** argv) {
  std::string program_name = argv[0];
  std::string usage = "Deletes a series of frames in a yuv file."
    " Only I420 is supported!\n"
    "Example usage:\n" + program_name +
    " --in_path=input.yuv --width=320 --height=240 --f=60 --interval=1 --l=120"
    " --out_path=edited_clip.yuv\n"
    "Command line flags:\n"
    "--in_path(string): Path and filename to the input file\n"
    "--width(int): Width in pixels of the frames in the input file."
    " Default: -1\n"
    "--height(int): Height in pixels of the frames in the input file."
    " Default: -1\n"
    "--f(int): First frame to cut.\n"
    " Default: -1\n"
    "--interval(int): Set to 1 if every frame between f and l should be "
    "deleted. Set it to 2 if every second frame should be deleted, "
    "and so on...Frame numbering between the limits start with 1 and frames "
    "read between and including the limits with number n where "
    "n % interval != 0 will be kept.\n"
    "Example: If the clip have frames with the numbers 1 to 10, and you set f=2"
    " , l=7 and interval=2, then the output clip will contain the frames with "
    " number 1, 2, 4, 6, 8, 9, 10."
    " Default: 1\n"
    "--l(int): Last frame to cut."
    " Default: -1\n"
    " --out_path(string): The output file to which frames are written."
    " Default: output.yuv\n";

  webrtc::test::CommandLineParser parser;

  // Init the parser and set the usage message
  parser.Init(argc, argv);
  parser.SetUsageMessage(usage);

  parser.SetFlag("in_path", "-1");
  parser.SetFlag("width", "-1");
  parser.SetFlag("height", "-1");
  parser.SetFlag("f", "-1");
  parser.SetFlag("interval", "1");
  parser.SetFlag("l", "-1");
  parser.SetFlag("out_path", "edited_output.yuv");
  parser.SetFlag("help", "false");

  parser.ProcessFlags();
  if (parser.GetFlag("help") == "true") {
    parser.PrintUsageMessage();
  }
  parser.PrintEnteredFlags();

  const char* in_path = parser.GetFlag("in_path").c_str();
  int width = strtol((parser.GetFlag("width")).c_str(), NULL, 10);
  int height = strtol((parser.GetFlag("height")).c_str(), NULL, 10);
  int first_frame_to_cut = strtol((parser.GetFlag("f")).c_str(), NULL, 10);
  int interval = strtol((parser.GetFlag("interval")).c_str(), NULL, 10);
  int last_frame_to_cut = strtol((parser.GetFlag("l")).c_str(), NULL, 10);

  const char* out_path = parser.GetFlag("out_path").c_str();

  if (!strcmp(in_path, "-1")) {
    fprintf(stderr, "You must specify a file to edit\n");
    return -1;
  }

  if (first_frame_to_cut <= 0 || last_frame_to_cut <= 0) {
    fprintf(stderr, "Error: You must specify which frames to cut!\n");
    return -2;
  }

  if (width <= 0 || height <= 0) {
    fprintf(stderr, "Error: width or height cannot be <= 0!\n");
    return -3;
  }

  return webrtc::CutFrames(in_path, width, height, first_frame_to_cut,
                             interval, last_frame_to_cut, out_path);
}

