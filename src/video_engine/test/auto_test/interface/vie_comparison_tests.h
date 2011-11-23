/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_COMPARISON_TESTS_H_
#define SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_COMPARISON_TESTS_H_

#include <string>

class ViEToFileRenderer;

// This class contains comparison tests, which will exercise video engine
// functionality and then run comparison tests on the result using PSNR and
// SSIM algorithms. These tests are intended mostly as sanity checks so that
// we know we are outputting roughly the right thing and not random noise or
// black screens.
//
// We will set up a fake ExternalCapture device which will pose as a webcam
// and read the input from the provided raw YUV file. Output will be written
// as a local preview in the local file renderer; the remote side output gets
// written to the provided remote file renderer.
//
// The local preview is a straight, unaltered copy of the input. This can be
// useful for comparisons if the test method contains several stages where the
// input is restarted between stages.
class ViEComparisonTests {
 public:
  // Test a typical simple call setup.
  void TestCallSetup(
      const std::string& i420_test_video_path,
      int width,
      int height,
      ViEToFileRenderer* local_file_renderer,
      ViEToFileRenderer* remote_file_renderer);

  // Tries testing the I420 and VP8 codecs in turn.
  void TestCodecs(
      const std::string& i420_video_file,
      int width,
      int height,
      ViEToFileRenderer* local_file_renderer,
      ViEToFileRenderer* remote_file_renderer);
};

#endif  // SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_COMPARISON_TESTS_H_
