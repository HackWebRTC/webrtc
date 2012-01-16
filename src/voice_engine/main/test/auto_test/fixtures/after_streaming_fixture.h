/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_STANDARD_AFTER_STREAMING_H_
#define SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_STANDARD_AFTER_STREAMING_H_

#include "after_initialization_fixture.h"
#include "resource_manager.h"

// This fixture will, in addition to the work done by its superclasses,
// create a channel and start playing a file through the fake microphone
// to simulate microphone input. The purpose is to make it convenient
// to write tests that require microphone input.
class AfterStreamingFixture : public AfterInitializationFixture {
 public:
  AfterStreamingFixture();
  virtual ~AfterStreamingFixture();

 protected:
  int             channel_;
  ResourceManager resource_manager_;

 private:
  void SetUpLocalPlayback();
  void StartPlaying(const std::string& input_file);
};


#endif  // SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_STANDARD_AFTER_STREAMING_H_
