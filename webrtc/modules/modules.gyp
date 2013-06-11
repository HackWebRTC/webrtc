# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../build/common.gypi',
    'audio_coding/codecs/cng/cng.gypi',
    'audio_coding/codecs/g711/g711.gypi',
    'audio_coding/codecs/g722/g722.gypi',
    'audio_coding/codecs/ilbc/ilbc.gypi',
    'audio_coding/codecs/isac/main/source/isac.gypi',
    'audio_coding/codecs/isac/fix/source/isacfix.gypi',
    'audio_coding/codecs/pcm16b/pcm16b.gypi',
    'audio_coding/main/source/audio_coding_module.gypi',
    'audio_coding/neteq/neteq.gypi',
    'audio_coding/neteq4/neteq.gypi',
    'audio_conference_mixer/source/audio_conference_mixer.gypi',
    'audio_device/audio_device.gypi',
    'audio_processing/audio_processing.gypi',
    'bitrate_controller/bitrate_controller.gypi',
    'desktop_capture/desktop_capture.gypi',
    'media_file/source/media_file.gypi',
    'pacing/pacing.gypi',
    'remote_bitrate_estimator/remote_bitrate_estimator.gypi',
    'rtp_rtcp/source/rtp_rtcp.gypi',
    'utility/source/utility.gypi',
    'video_coding/codecs/i420/main/source/i420.gypi',
    'video_coding/main/source/video_coding.gypi',
    'video_capture/video_capture.gypi',
    'video_processing/main/source/video_processing.gypi',
    'video_render/video_render.gypi',
  ],
  'conditions': [
    ['include_opus==1', {
      'includes': ['audio_coding/codecs/opus/opus.gypi',],
    }],
    ['include_tests==1', {
      'includes': [
        'audio_coding/codecs/isac/isac_test.gypi',
        'audio_coding/codecs/isac/isacfix_test.gypi',
        'audio_processing/audio_processing_tests.gypi',
        'rtp_rtcp/test/testFec/test_fec.gypi',
        'video_coding/main/source/video_coding_test.gypi',
        'video_coding/codecs/test/video_codecs_test_framework.gypi',
        'video_coding/codecs/tools/video_codecs_tools.gypi',
        'video_processing/main/test/vpm_tests.gypi',
      ], # includes
     'variables': {
        'conditions': [
          # Desktop capturer is supported only on Windows, OSX and Linux.
          ['OS=="win" or OS=="mac" or OS=="linux"', {
            'desktop_capture_supported%': 1,
          }, {
            'desktop_capture_supported%': 0,
          }],
        ],
      },
      'targets': [
        {
          'target_name': 'modules_unittests',
          'type': 'executable',
          'dependencies': [
            'bitrate_controller',
            'desktop_capture',
            'media_file',
            'paced_sender',
            'remote_bitrate_estimator',
            'rtp_rtcp',
            'webrtc_utility',
            '<(DEPTH)/testing/gmock.gyp:gmock',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/test/test.gyp:test_support_main',
          ],
          'sources': [
            'module_common_types_unittest.cc',
            'bitrate_controller/bitrate_controller_unittest.cc',
            'desktop_capture/desktop_region_unittest.cc',
            'desktop_capture/differ_block_unittest.cc',
            'desktop_capture/differ_unittest.cc',
            'desktop_capture/screen_capturer_helper_unittest.cc',
            'desktop_capture/screen_capturer_mac_unittest.cc',
            'desktop_capture/screen_capturer_mock_objects.h',
            'desktop_capture/screen_capturer_unittest.cc',
            'desktop_capture/window_capturer_unittest.cc',
            "desktop_capture/win/cursor_unittest.cc",
            "desktop_capture/win/cursor_unittest_resources.h",
            "desktop_capture/win/cursor_unittest_resources.rc",
            'media_file/source/media_file_unittest.cc',
            'pacing/paced_sender_unittest.cc',
            'remote_bitrate_estimator/include/mock/mock_remote_bitrate_observer.h',
            'remote_bitrate_estimator/bitrate_estimator_unittest.cc',
            'remote_bitrate_estimator/remote_bitrate_estimator_multi_stream_unittest.cc',
            'remote_bitrate_estimator/remote_bitrate_estimator_single_stream_unittest.cc',
            'remote_bitrate_estimator/remote_bitrate_estimator_unittest_helper.cc',
            'remote_bitrate_estimator/remote_bitrate_estimator_unittest_helper.h',
            'remote_bitrate_estimator/rtp_to_ntp_unittest.cc',
            'rtp_rtcp/source/mock/mock_rtp_payload_strategy.h',
            'rtp_rtcp/source/mock/mock_rtp_receiver_video.h',
            'rtp_rtcp/source/fec_test_helper.cc',
            'rtp_rtcp/source/fec_test_helper.h',
            'rtp_rtcp/source/nack_rtx_unittest.cc',
            'rtp_rtcp/source/producer_fec_unittest.cc',
            'rtp_rtcp/source/receiver_fec_unittest.cc',
            'rtp_rtcp/source/rtcp_format_remb_unittest.cc',
            'rtp_rtcp/source/rtcp_sender_unittest.cc',
            'rtp_rtcp/source/rtcp_receiver_unittest.cc',
            'rtp_rtcp/source/rtp_fec_unittest.cc',
            'rtp_rtcp/source/rtp_format_vp8_unittest.cc',
            'rtp_rtcp/source/rtp_format_vp8_test_helper.cc',
            'rtp_rtcp/source/rtp_format_vp8_test_helper.h',
            'rtp_rtcp/source/rtp_packet_history_unittest.cc',
            'rtp_rtcp/source/rtp_payload_registry_unittest.cc',
            'rtp_rtcp/source/rtp_utility_unittest.cc',
            'rtp_rtcp/source/rtp_header_extension_unittest.cc',
            'rtp_rtcp/source/rtp_sender_unittest.cc',
            'rtp_rtcp/source/vp8_partition_aggregator_unittest.cc',
            'rtp_rtcp/test/testAPI/test_api.cc',
            'rtp_rtcp/test/testAPI/test_api.h',
            'rtp_rtcp/test/testAPI/test_api_audio.cc',
            'rtp_rtcp/test/testAPI/test_api_rtcp.cc',
            'rtp_rtcp/test/testAPI/test_api_video.cc',
            'utility/source/audio_frame_operations_unittest.cc',
          ],
          'conditions': [
            # Run screen/window capturer tests only on platforms where they are
            # supported.
            ['desktop_capture_supported==1', {
              'sources!': [
                'desktop_capture/screen_capturer_helper_unittest.cc',
                'desktop_capture/screen_capturer_mac_unittest.cc',
                'desktop_capture/screen_capturer_mock_objects.h',
                'desktop_capture/screen_capturer_unittest.cc',
                'desktop_capture/window_capturer_unittest.cc',
              ],
            }],
          ],
          # Disable warnings to enable Win64 build, issue 1323.
          'msvs_disabled_warnings': [
            4267,  # size_t to int truncation.
          ],
        },
      ],
    }], # include_tests
  ], # conditions
}
