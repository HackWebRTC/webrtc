/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include "modules/audio_coding/neteq/tools/neteq_test.h"
#include "modules/audio_coding/neteq/tools/neteq_test_factory.h"

int main(int argc, char* argv[]) {
  webrtc::test::NetEqTestFactory factory;
  std::unique_ptr<webrtc::test::NetEqTest> test =
      factory.InitializeTest(argc, argv);
  test->Run();
  return 0;
}
