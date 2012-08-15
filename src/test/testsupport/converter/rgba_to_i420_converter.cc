/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <cstdio>

#include "google/gflags.h"
#include "test/testsupport/converter/converter.h"

DEFINE_int32(width, -1, "Width in pixels of the frames in the input file.");
DEFINE_int32(height, -1, "Height in pixels of the frames in the input file.");
DEFINE_string(frames_dir, ".", "The path to the directory where the frames "
              "reside");
DEFINE_string(output_file, "./output.yuv", "The output file to which frames are"
              " written");
DEFINE_bool(delete_frames, false, "Whether or not to delete the input frames "
            "after the conversion.");

/*
 * A command-line tool based on libyuv to convert a set of RGBA files to a YUV
 * video.
 * Usage:
 * rgba_to_i420_converter --frames_dir=<directory_to_rgba_frames>
 * --output_file=<output_yuv_file> --width=<width_of_input_frames>
 * --height=<height_of_input_frames>
 */
int main(int argc, char** argv) {
  std::string program_name = argv[0];
  std::string usage = "Converts RGBA raw image files to I420 frames for YUV.\n"
    "Run " + program_name + " --helpshort for usage.\n"
    "Example usage:\n" + program_name +
    " --frames_dir=. --output_file=output.yuv --width=320 --height=240\n" +
    "IMPORTANT: If you pass the --delete_frames command line parameter, the " +
    "tool will delete the input frames after conversion.";
  google::SetUsageMessage(usage);

  google::ParseCommandLineFlags(&argc, &argv, true);

  fprintf(stdout, "You entered the following flags: frames_dir=%s, "
          "output_file=%s, width=%d, height=%d, delete_frames=%d\n",
          FLAGS_frames_dir.c_str(), FLAGS_output_file.c_str(),
          FLAGS_width, FLAGS_height, FLAGS_delete_frames);

  webrtc::test::Converter converter(FLAGS_width, FLAGS_height);
  bool success = converter.ConvertRGBAToI420Video(FLAGS_frames_dir,
                                                  FLAGS_output_file,
                                                  FLAGS_delete_frames);

  if (success) {
    fprintf(stdout, "Successful conversion of RGBA frames to YUV video!\n");
    return 0;
  } else {
    fprintf(stdout, "Unsuccessful conversion of RGBA frames to YUV video!\n");
    return -1;
  }
}

