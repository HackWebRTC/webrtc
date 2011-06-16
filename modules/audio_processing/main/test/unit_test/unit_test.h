/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_MAIN_TEST_UNIT_TEST_UNIT_TEST_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_MAIN_TEST_UNIT_TEST_UNIT_TEST_H_

#include <gtest/gtest.h>

namespace webrtc {

class AudioProcessing;
class AudioFrame;

class ApmTest : public ::testing::Test {
 protected:
  ApmTest();
  virtual void SetUp();
  virtual void TearDown();

  webrtc::AudioProcessing* apm_;
  FILE* far_file_;
  FILE* near_file_;
  FILE* stat_file_;
  AudioFrame* frame_;
  AudioFrame* reverse_frame_;
};
} // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_MAIN_TEST_UNIT_TEST_UNIT_TEST_H_
