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
        'video_replay',
        'webrtc_perf_tests',
      ],
    },
    {
      'target_name': 'video_loopback',
      'type': 'executable',
      'sources': [
        'test/mac/run_test.mm',
        'test/run_test.cc',
        'test/run_test.h',
        'video/loopback.cc',
      ],
      'conditions': [
        ['OS=="mac"', {
          'sources!': [
            'test/run_test.cc',
          ],
        }],
      ],
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
        'test/webrtc_test_common.gyp:webrtc_test_common',
        'test/webrtc_test_common.gyp:webrtc_test_renderer',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:field_trial_default',
        'webrtc',
      ],
    },
    {
      'target_name': 'video_replay',
      'type': 'executable',
      'sources': [
        'test/mac/run_test.mm',
        'test/run_test.cc',
        'test/run_test.h',
        'video/replay.cc',
      ],
      'conditions': [
        ['OS=="mac"', {
          'sources!': [
            'test/run_test.cc',
          ],
        }],
      ],
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
        'system_wrappers/source/system_wrappers.gyp:field_trial_default',
        'test/webrtc_test_common.gyp:webrtc_test_common',
        'test/webrtc_test_common.gyp:webrtc_test_renderer',
        'webrtc',
      ],
    },
    {
      'target_name': 'video_engine_tests',
      'type': '<(gtest_target_type)',
      'sources': [
        'video/bitrate_estimator_tests.cc',
        'video/end_to_end_tests.cc',
        'video/send_statistics_proxy_unittest.cc',
        'video/video_send_stream_tests.cc',
        'test/common_unittest.cc',
        'test/testsupport/metrics/video_metrics_unittest.cc',
      ],
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'modules/modules.gyp:rtp_rtcp',
        'test/metrics.gyp:metrics',
        'test/webrtc_test_common.gyp:webrtc_test_common',
        'test/test.gyp:test_main',
        'test/webrtc_test_common.gyp:webrtc_test_video_render_dependencies',
        'webrtc',
      ],
      'conditions': [
        # TODO(henrike): remove build_with_chromium==1 when the bots are
        # using Chromium's buildbots.
        ['build_with_chromium==1 and OS=="android"', {
          'dependencies': [
            '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
      ],
    },
    {
      'target_name': 'webrtc_perf_tests',
      'type': '<(gtest_target_type)',
      'sources': [
        'modules/audio_coding/neteq/test/neteq_performance_unittest.cc',
        'video/call_perf_tests.cc',
        'video/full_stack.cc',
        'video/rampup_tests.cc',
        'video/rampup_tests.h',
      ],
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'modules/modules.gyp:neteq_test_support',  # Needed by neteq_performance_unittest.
        'modules/modules.gyp:rtp_rtcp',
        'test/webrtc_test_common.gyp:webrtc_test_common',
        'test/test.gyp:test_main',
        'test/webrtc_test_common.gyp:webrtc_test_video_render_dependencies',
        'webrtc',
      ],
      'conditions': [
        # TODO(henrike): remove build_with_chromium==1 when the bots are
        # using Chromium's buildbots.
        ['build_with_chromium==1 and OS=="android"', {
          'dependencies': [
            '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    # TODO(henrike): remove build_with_chromium==1 when the bots are using
    # Chromium's buildbots.
    ['build_with_chromium==1 and OS=="android"', {
      'targets': [
        {
          'target_name': 'video_engine_tests_apk_target',
          'type': 'none',
          'dependencies': [
            '<(apk_tests_path):video_engine_tests_apk',
          ],
        },
        {
          'target_name': 'webrtc_perf_tests_apk_target',
          'type': 'none',
          'dependencies': [
            '<(apk_tests_path):webrtc_perf_tests_apk',
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
        {
          'target_name': 'webrtc_perf_tests_run',
          'type': 'none',
          'dependencies': [
            'webrtc_perf_tests',
          ],
          'includes': [
            'build/isolate.gypi',
            'webrtc_perf_tests.isolate',
          ],
          'sources': [
            'webrtc_perf_tests.isolate',
          ],
        },
      ],
    }],
  ],
}
