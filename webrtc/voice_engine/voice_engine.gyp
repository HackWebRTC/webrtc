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
  ],
  'targets': [
    {
      'target_name': 'voice_engine',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/api/api.gyp:call_api',
        '<(webrtc_root)/base/base.gyp:rtc_base_approved',
        '<(webrtc_root)/common.gyp:webrtc_common',
        '<(webrtc_root)/common_audio/common_audio.gyp:common_audio',
        '<(webrtc_root)/modules/modules.gyp:audio_coding_module',
        '<(webrtc_root)/modules/modules.gyp:audio_conference_mixer',
        '<(webrtc_root)/modules/modules.gyp:audio_device',
        '<(webrtc_root)/modules/modules.gyp:audio_processing',
        '<(webrtc_root)/modules/modules.gyp:bitrate_controller',
        '<(webrtc_root)/modules/modules.gyp:media_file',
        '<(webrtc_root)/modules/modules.gyp:paced_sender',
        '<(webrtc_root)/modules/modules.gyp:rtp_rtcp',
        '<(webrtc_root)/modules/modules.gyp:webrtc_utility',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
        '<(webrtc_root)/webrtc.gyp:rtc_event_log',
        'level_indicator',
      ],
      'export_dependent_settings': [
        '<(webrtc_root)/modules/modules.gyp:audio_coding_module',
      ],
      'sources': [
        'include/voe_audio_processing.h',
        'include/voe_base.h',
        'include/voe_codec.h',
        'include/voe_errors.h',
        'include/voe_external_media.h',
        'include/voe_file.h',
        'include/voe_hardware.h',
        'include/voe_neteq_stats.h',
        'include/voe_network.h',
        'include/voe_rtp_rtcp.h',
        'include/voe_video_sync.h',
        'include/voe_volume_control.h',
        'channel.cc',
        'channel.h',
        'channel_manager.cc',
        'channel_manager.h',
        'channel_proxy.cc',
        'channel_proxy.h',
        'monitor_module.cc',
        'monitor_module.h',
        'network_predictor.cc',
        'network_predictor.h',
        'output_mixer.cc',
        'output_mixer.h',
        'shared_data.cc',
        'shared_data.h',
        'statistics.cc',
        'statistics.h',
        'transmit_mixer.cc',
        'transmit_mixer.h',
        'utility.cc',
        'utility.h',
        'voe_audio_processing_impl.cc',
        'voe_audio_processing_impl.h',
        'voe_base_impl.cc',
        'voe_base_impl.h',
        'voe_codec_impl.cc',
        'voe_codec_impl.h',
        'voe_external_media_impl.cc',
        'voe_external_media_impl.h',
        'voe_file_impl.cc',
        'voe_file_impl.h',
        'voe_hardware_impl.cc',
        'voe_hardware_impl.h',
        'voe_neteq_stats_impl.cc',
        'voe_neteq_stats_impl.h',
        'voe_network_impl.cc',
        'voe_network_impl.h',
        'voe_rtp_rtcp_impl.cc',
        'voe_rtp_rtcp_impl.h',
        'voe_video_sync_impl.cc',
        'voe_video_sync_impl.h',
        'voe_volume_control_impl.cc',
        'voe_volume_control_impl.h',
        'voice_engine_defines.h',
        'voice_engine_impl.cc',
        'voice_engine_impl.h',
      ],
    },
    {
      'target_name': 'level_indicator',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/base/base.gyp:rtc_base_approved',
        '<(webrtc_root)/common.gyp:webrtc_common',
        '<(webrtc_root)/common_audio/common_audio.gyp:common_audio',
      ],
      'sources': [
        'level_indicator.cc',
        'level_indicator.h',
      ]
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'defines': ['WEBRTC_DRIFT_COMPENSATION_SUPPORTED',],
    }],
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'channel_transport',
          'type': 'static_library',
          'dependencies': [
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(webrtc_root)/common.gyp:webrtc_common',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
          ],
          'sources': [
            'test/channel_transport/channel_transport.cc',
            'test/channel_transport/channel_transport.h',
            'test/channel_transport/traffic_control_win.cc',
            'test/channel_transport/traffic_control_win.h',
            'test/channel_transport/udp_socket_manager_posix.cc',
            'test/channel_transport/udp_socket_manager_posix.h',
            'test/channel_transport/udp_socket_manager_wrapper.cc',
            'test/channel_transport/udp_socket_manager_wrapper.h',
            'test/channel_transport/udp_socket_posix.cc',
            'test/channel_transport/udp_socket_posix.h',
            'test/channel_transport/udp_socket_wrapper.cc',
            'test/channel_transport/udp_socket_wrapper.h',
            'test/channel_transport/udp_socket2_manager_win.cc',
            'test/channel_transport/udp_socket2_manager_win.h',
            'test/channel_transport/udp_socket2_win.cc',
            'test/channel_transport/udp_socket2_win.h',
            'test/channel_transport/udp_transport.h',
            'test/channel_transport/udp_transport_impl.cc',
            'test/channel_transport/udp_transport_impl.h',
          ],
          'msvs_disabled_warnings': [
            4302,  # cast truncation
          ],
          'conditions': [
            ['OS=="win" and clang==1', {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'AdditionalOptions': [
                    # Disable warnings failing when compiling with Clang on Windows.
                    # https://bugs.chromium.org/p/webrtc/issues/detail?id=5366
                    '-Wno-parentheses-equality',
                    '-Wno-reorder',
                    '-Wno-tautological-constant-out-of-range-compare',
                    '-Wno-unused-private-field',
                  ],
                },
              },
            }],
          ],  # conditions.
        },
        {
          # command line test that should work on linux/mac/win
          'target_name': 'voe_cmd_test',
          'type': 'executable',
          'dependencies': [
            'channel_transport',
            'voice_engine',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers_default',
            '<(webrtc_root)/test/test.gyp:test_support',
            '<(webrtc_root)/webrtc.gyp:rtc_event_log',
          ],
          'sources': [
            'test/cmd_test/voe_cmd_test.cc',
          ],
        },
      ], # targets
    }], # include_tests==1
  ], # conditions
}
