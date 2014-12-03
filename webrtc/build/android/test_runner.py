#!/usr/bin/env python
# Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""
Runs tests on Android devices.

This script exists to avoid WebRTC being broken by changes in the Chrome Android
test execution toolchain.
"""

import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.join(SCRIPT_DIR, os.pardir, os.pardir, os.pardir)
CHROMIUM_BUILD_ANDROID_DIR = os.path.join(ROOT_DIR, 'build', 'android')
sys.path.insert(0, CHROMIUM_BUILD_ANDROID_DIR)


import test_runner
from pylib.gtest import gtest_config


def main():
  # Override the stable test suites with the WebRTC tests.
  gtest_config.STABLE_TEST_SUITES = [
    'audio_decoder_unittests',
    'common_audio_unittests',
    'common_video_unittests',
    'modules_tests',
    'modules_unittests',
    'system_wrappers_unittests',
    'test_support_unittests',
    'tools_unittests',
    'video_capture_tests',
    'video_engine_tests',
    'video_engine_core_unittests',
    'voice_engine_unittests',
    'webrtc_perf_tests',
  ]
  return test_runner.main()

if __name__ == '__main__':
  sys.exit(main())
