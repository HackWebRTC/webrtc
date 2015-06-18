# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'video_engine_core',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/common.gyp:webrtc_common',

        # common_video
       '<(webrtc_root)/common_video/common_video.gyp:common_video',

        # ModulesShared
        '<(webrtc_root)/modules/modules.gyp:rtp_rtcp',
        '<(webrtc_root)/modules/modules.gyp:webrtc_utility',

        # ModulesVideo
        '<(webrtc_root)/modules/modules.gyp:bitrate_controller',
        '<(webrtc_root)/modules/modules.gyp:video_capture_module',
        '<(webrtc_root)/modules/modules.gyp:webrtc_video_coding',
        '<(webrtc_root)/modules/modules.gyp:video_processing',
        '<(webrtc_root)/modules/modules.gyp:video_render_module',

        # VoiceEngine
        '<(webrtc_root)/voice_engine/voice_engine.gyp:voice_engine',

        # system_wrappers
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
      'sources': [
        # headers
        'call_stats.h',
        'encoder_state_feedback.h',
        'overuse_frame_detector.h',
        'payload_router.h',
        'report_block_stats.h',
        'stream_synchronization.h',
        'vie_defines.h',
        'vie_remb.h',
        'vie_capturer.h',
        'vie_channel.h',
        'vie_channel_group.h',
        'vie_encoder.h',
        'vie_receiver.h',
        'vie_sync_module.h',

        # ViE
        'call_stats.cc',
        'encoder_state_feedback.cc',
        'overuse_frame_detector.cc',
        'payload_router.cc',
        'report_block_stats.cc',
        'stream_synchronization.cc',
        'vie_capturer.cc',
        'vie_channel.cc',
        'vie_channel_group.cc',
        'vie_encoder.cc',
        'vie_receiver.cc',
        'vie_remb.cc',
        'vie_sync_module.cc',
      ], # source
    },
  ], # targets
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'video_engine_core_unittests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            'video_engine_core',
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
            'vie_capturer_unittest.cc',
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
    }], # include_tests
  ], # conditions
}
