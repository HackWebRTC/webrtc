# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
{
  'targets': [
    {
      'target_name': 'webrtc_tests',
      'type': 'none',
      'dependencies': [
        'video_engine_tests',
        'video_loopback',
      ],
    },
    {
      'target_name': 'video_loopback',
      'type': 'executable',
      'sources': [
        'video/loopback.cc',
        'test/test_main.cc',
      ],
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'test/webrtc_test_common.gyp:webrtc_test_common',
        'webrtc',
      ],
    },
    {
      'target_name': 'video_engine_tests',
      'type': '<(gtest_target_type)',
      'sources': [
        'video/call_tests.cc',
        'video/full_stack.cc',
        'video/rampup_tests.cc',
        'video/video_send_stream_tests.cc',
        'test/common_unittest.cc',
        'test/test_main.cc',
      ],
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
        'modules/modules.gyp:rtp_rtcp',
        'test/webrtc_test_common.gyp:webrtc_test_common',
        'webrtc',
      ],
    },
  ],
  'conditions': [
    # TODO(henrike): remove build_with_chromium==1 when the bots are using
    # Chromium's buildbots.
    ['build_with_chromium==1 and OS=="android" and gtest_target_type=="shared_library"', {
      'targets': [
        {
          'target_name': 'video_engine_tests_apk_target',
          'type': 'none',
          'dependencies': [
            '<(apk_tests_path):video_engine_tests_apk',
          ],
        },
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'video_engine_tests_run',
          'type': 'none',
          'dependencies': [
            'video_engine_tests',
          ],
          'includes': [
            'build/isolate.gypi',
            'video_engine_tests.isolate',
          ],
          'sources': [
            'video_engine_tests.isolate',
          ],
        },
      ],
    }],
  ],
}
