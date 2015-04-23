/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/h264_sps_parser.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace webrtc {

// Example SPS can be generated with ffmpeg. Here's an example set of commands,
// runnable on OS X:
// 1) Generate a video, from the camera:
// ffmpeg -f avfoundation -i "0" -video_size 640x360 camera.mov
//
// 2) Scale the video to the desired size:
// ffmpeg -i camera.mov -vf scale=640x360 scaled.mov
//
// 3) Get just the H.264 bitstream in AnnexB:
// ffmpeg -i scaled.mov -vcodec copy -vbsf h264_mp4toannexb -an out.h264
//
// 4) Open out.h264 and find the SPS, generally everything between the first
// two start codes (0 0 0 1 or 0 0 1). The first byte should be 0x67,
// which should be stripped out before being passed to the parser.

TEST(H264SpsParserTest, TestSampleSPSHdLandscape) {
  // SPS for a 1280x720 camera capture from ffmpeg on osx. Contains
  // emulation bytes but no cropping.
  const uint8 buffer[] = {0x7A, 0x00, 0x1F, 0xBC, 0xD9, 0x40, 0x50, 0x05,
                          0xBA, 0x10, 0x00, 0x00, 0x03, 0x00, 0xC0, 0x00,
                          0x00, 0x2A, 0xE0, 0xF1, 0x83, 0x19, 0x60};
  H264SpsParser parser = H264SpsParser(buffer, ARRAY_SIZE(buffer));
  EXPECT_TRUE(parser.Parse());
  EXPECT_EQ(1280u, parser.width());
  EXPECT_EQ(720u, parser.height());
}

TEST(H264SpsParserTest, TestSampleSPSVgaLandscape) {
  // SPS for a 640x360 camera capture from ffmpeg on osx. Contains emulation
  // bytes and cropping (360 isn't divisible by 16).
  const uint8 buffer[] = {0x7A, 0x00, 0x1E, 0xBC, 0xD9, 0x40, 0xA0, 0x2F,
                          0xF8, 0x98, 0x40, 0x00, 0x00, 0x03, 0x01, 0x80,
                          0x00, 0x00, 0x56, 0x83, 0xC5, 0x8B, 0x65, 0x80};
  H264SpsParser parser = H264SpsParser(buffer, ARRAY_SIZE(buffer));
  EXPECT_TRUE(parser.Parse());
  EXPECT_EQ(640u, parser.width());
  EXPECT_EQ(360u, parser.height());
}

TEST(H264SpsParserTest, TestSampleSPSWeirdResolution) {
  // SPS for a 200x400 camera capture from ffmpeg on osx. Horizontal and
  // veritcal crop (neither dimension is divisible by 16).
  const uint8 buffer[] = {0x7A, 0x00, 0x0D, 0xBC, 0xD9, 0x43, 0x43, 0x3E,
                          0x5E, 0x10, 0x00, 0x00, 0x03, 0x00, 0x60, 0x00,
                          0x00, 0x15, 0xA0, 0xF1, 0x42, 0x99, 0x60};
  H264SpsParser parser = H264SpsParser(buffer, ARRAY_SIZE(buffer));
  EXPECT_TRUE(parser.Parse());
  EXPECT_EQ(200u, parser.width());
  EXPECT_EQ(400u, parser.height());
}

}  // namespace webrtc
