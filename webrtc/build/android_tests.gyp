# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This file exists in two versions. A no-op version under
# webrtc/build/android_tests_noop.gyp and this one. This gyp file builds the
# tests (for Android) assuming that WebRTC is built inside a Chromium
# workspace. The no-op version is included when building WebRTC without
# Chromium. This is a workaround for the fact that 'includes' don't expand
# variables and that the relative location of apk_test.gypi is different for
# WebRTC when built as part of Chromium and when it is built without Chromium.
{
  'includes': [
    'common.gypi',
  ],
  'targets': [
    {
      'target_name': 'android_junit_tests',
      'type': 'none',
      'dependencies': [
        '<(webrtc_root)/api/api_java.gyp:libjingle_peerconnection_java',
        '<(DEPTH)/base/base.gyp:base_java',
        '<(DEPTH)/base/base.gyp:base_java_test_support',
        '<(DEPTH)/base/base.gyp:base_junit_test_support',
      ],
      'variables': {
        'main_class': 'org.chromium.testing.local.JunitTestMain',
        'src_paths': [
          '../androidjunit/',
        ],
        'test_type': 'junit',
        'wrapper_script_name': 'helper/<(_target_name)',
      },
      'includes': [
        '../../build/android/test_runner.gypi',
        '../../build/host_jar.gypi',
      ],
    },
  ],
}
