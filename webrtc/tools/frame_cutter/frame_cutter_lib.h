/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TOOLS_FRAME_CUTTER_FRAME_CUTTER_H_
#define WEBRTC_TOOLS_FRAME_CUTTER_FRAME_CUTTER_H_

#include <string>

namespace webrtc {

// Frame numbering starts at 1. The set of frames to be cut includes the frame
// with the number: first_frame_to_cut and last_frame_to_cut. I.e if one clip
// has 10 frames (1 to 10), and you specify first_frame_to_cut = 4,
// last_frame_to_cut = 7 and interval = 1, then you will get a clip that
// contains frame 1, 2, 3, 8, 9 and 10.
// Interval specifies with what interval frames should be cut. I.e if one clip
// has 10 frames (1 to 10), and you specify first_frame_to_cut = 1,
// last_frame_to_cut = 10 and interval = 3, then you will get a clip that
// contains frame 1, 2, 4, 5, 7, 8, 10.
int CutFrames(const std::string& in_path, int width, int height,
                int first_frame_to_cut, int interval, int last_frame_to_cut,
                const std::string& out_path);
}

#endif  // WEBRTC_TOOLS_FRAME_CUTTER_FRAME_CUTTER_H_
