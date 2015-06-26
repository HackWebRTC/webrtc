# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'video_engine_core_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '<(webrtc_root)/webrtc.gyp:webrtc',
        '<(webrtc_root)/modules/modules.gyp:video_capture_module_internal_impl',
        '<(webrtc_root)/modules/modules.gyp:video_render_module_internal_impl',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(webrtc_root)/test/test.gyp:test_support_main',
      ],
      'sources': [
        'call_stats_unittest.cc',
        'encoder_state_feedback_unittest.cc',
        'overuse_frame_detector_unittest.cc',
        'payload_router_unittest.cc',
        'report_block_stats_unittest.cc',
        'stream_synchronization_unittest.cc',
        'vie_codec_unittest.cc',
        'vie_remb_unittest.cc',
      ],
      'conditions': [
        ['OS=="android"', {
          'dependencies': [
            '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
      ],
    },
  ], # targets
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'video_engine_core_unittests_apk_target',
          'type': 'none',
          'dependencies': [
            '<(apk_tests_path):video_engine_core_unittests_apk',
          ],
        },
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'video_engine_core_unittests_run',
          'type': 'none',
          'dependencies': [
            'video_engine_core_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'video_engine_core_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
