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

#include <string>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

using std::string;

namespace webrtc {

int FrameCutter(const string& in_path, int width, int height,
                int first_frame_to_cut, int last_frame_to_cut,
                const string& out_path) {
  if (last_frame_to_cut < first_frame_to_cut) {
    fprintf(stderr, "The set of frames to cut is empty! (l < f)\n");
    return -10;
  }

  FILE* in_fid = fopen(in_path.c_str() , "r");
  if (!in_fid) {
    fprintf(stderr, "Could not read input file: %s.\n", in_path.c_str());
    return -11;
  }

  // Frame size of I420.
  int frame_length = CalcBufferSize(kI420, width, height);

  webrtc::scoped_array<uint8_t> temp_buffer(new uint8_t[frame_length]);

  FILE* out_fid = fopen(out_path.c_str(), "w");

  if (!out_fid) {
    fprintf(stderr, "Could not open output file: %s.\n", out_path.c_str());
    return -12;
  }

  int num_frames_read = 0;
  int num_bytes_read;

  while ((num_bytes_read = fread(temp_buffer.get(), 1, frame_length, in_fid))
      == frame_length) {
    if ((num_frames_read < first_frame_to_cut) ||
        (last_frame_to_cut < num_frames_read)) {
      fwrite(temp_buffer.get(), 1, frame_length, out_fid);
      num_frames_read++;
    } else {
      num_frames_read++;
    }
  }
  if (num_bytes_read > 0 && num_bytes_read < frame_length) {
    printf("Frame to small! Last frame truncated.\n");
  }

  fclose(in_fid);
  fclose(out_fid);

  printf("Done editing!\n");
  return 0;
}
}

