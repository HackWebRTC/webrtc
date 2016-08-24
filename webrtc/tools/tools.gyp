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
      'target_name': 'video_quality_analysis',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
      ],
      'export_dependent_settings': [
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
      ],
      'sources': [
        'frame_analyzer/video_quality_analysis.h',
        'frame_analyzer/video_quality_analysis.cc',
      ],
    }, # video_quality_analysis
    {
      'target_name': 'frame_analyzer',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/tools/internal_tools.gyp:command_line_parser',
        'video_quality_analysis',
      ],
      'sources': [
        'frame_analyzer/frame_analyzer.cc',
      ],
    }, # frame_analyzer
    {
      'target_name': 'psnr_ssim_analyzer',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/tools/internal_tools.gyp:command_line_parser',
        'video_quality_analysis',
      ],
      'sources': [
        'psnr_ssim_analyzer/psnr_ssim_analyzer.cc',
      ],
    }, # psnr_ssim_analyzer
    {
      'target_name': 'rgba_to_i420_converter',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
        '<(webrtc_root)/tools/internal_tools.gyp:command_line_parser',
      ],
      'sources': [
        'converter/converter.h',
        'converter/converter.cc',
        'converter/rgba_to_i420_converter.cc',
      ],
    }, # rgba_to_i420_converter
    {
      'target_name': 'frame_editing_lib',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
      ],
      'sources': [
        'frame_editing/frame_editing_lib.cc',
        'frame_editing/frame_editing_lib.h',
      ],
      # Disable warnings to enable Win64 build, issue 1323.
      'msvs_disabled_warnings': [
        4267,  # size_t to int truncation.
      ],
    }, # frame_editing_lib
    {
      'target_name': 'frame_editor',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/tools/internal_tools.gyp:command_line_parser',
        'frame_editing_lib',
      ],
      'sources': [
        'frame_editing/frame_editing.cc',
      ],
    }, # frame_editing
    {
      'target_name': 'force_mic_volume_max',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/voice_engine/voice_engine.gyp:voice_engine',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers_default',
      ],
      'sources': [
        'force_mic_volume_max/force_mic_volume_max.cc',
      ],
    }, # force_mic_volume_max
  ],
  'conditions': [
    ['enable_protobuf==1', {
      'targets': [
        {
          'target_name': 'graph_proto',
          'type': 'static_library',
          'sources': [
            'event_log_visualizer/graph.proto',
          ],
          'variables': {
            'proto_in_dir': 'event_log_visualizer',
            'proto_out_dir': 'webrtc/tools/event_log_visualizer',
          },
          'includes': ['../build/protoc.gypi'],
        },
        {
          # RTC event log visualization library
          'target_name': 'event_log_visualizer_utils',
          'type': 'static_library',
          'dependencies': [
            '<(webrtc_root)/webrtc.gyp:rtc_event_log',
            '<(webrtc_root)/webrtc.gyp:rtc_event_log_parser',
            '<(webrtc_root)/modules/modules.gyp:congestion_controller',
            '<(webrtc_root)/modules/modules.gyp:rtp_rtcp',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:metrics_default',
            ':graph_proto',
          ],
          'sources': [
            'event_log_visualizer/analyzer.cc',
            'event_log_visualizer/analyzer.h',
            'event_log_visualizer/plot_base.cc',
            'event_log_visualizer/plot_base.h',
            'event_log_visualizer/plot_protobuf.cc',
            'event_log_visualizer/plot_protobuf.h',
            'event_log_visualizer/plot_python.cc',
            'event_log_visualizer/plot_python.h',
          ],
          'export_dependent_settings': [
            '<(webrtc_root)/webrtc.gyp:rtc_event_log_parser',
            ':graph_proto',
          ],
        },
      ],
    }],
    ['enable_protobuf==1 and include_tests==1', {
      # TODO(terelius): This tool requires the include_test condition to
      # prevent build errors when gflags isn't found in downstream projects.
      # There should be a cleaner way to do this. The tool is not test related.
      'targets': [
        {
          # Command line tool for RTC event log visualization
          'target_name': 'event_log_visualizer',
          'type': 'executable',
          'dependencies': [
            'event_log_visualizer_utils',
            '<(webrtc_root)/test/test.gyp:field_trial',
            '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
          ],
          'sources': [
            'event_log_visualizer/main.cc',
          ],
        },
      ],
    }],
    ['include_tests==1', {
      'targets' : [
        {
          'target_name': 'agc_test_utils',
          'type': 'static_library',
          'sources': [
            'agc/test_utils.cc',
            'agc/test_utils.h',
          ],
        },
        {
          'target_name': 'agc_harness',
          'type': 'executable',
          'dependencies': [
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers_default',
            '<(webrtc_root)/test/test.gyp:channel_transport',
            '<(webrtc_root)/test/test.gyp:test_support',
            '<(webrtc_root)/voice_engine/voice_engine.gyp:voice_engine',
          ],
          'sources': [
            'agc/agc_harness.cc',
          ],
        },  # agc_harness
        {
          'target_name': 'activity_metric',
          'type': 'executable',
          'dependencies': [
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
            '<(webrtc_root)/modules/modules.gyp:audio_processing',
          ],
          'sources': [
            'agc/activity_metric.cc',
          ],
        },  # activity_metric
        {
          'target_name': 'audio_e2e_harness',
          'type': 'executable',
          'dependencies': [
            '<(webrtc_root)/test/test.gyp:channel_transport',
            '<(webrtc_root)/voice_engine/voice_engine.gyp:voice_engine',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers_default',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
          ],
          'sources': [
            'e2e_quality/audio/audio_e2e_harness.cc',
          ],
        }, # audio_e2e_harness
        {
          'target_name': 'tools_unittests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            'frame_editing_lib',
            'video_quality_analysis',
            '<(webrtc_root)/tools/internal_tools.gyp:command_line_parser',
            '<(webrtc_root)/test/test.gyp:test_support_main',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'sources': [
            'simple_command_line_parser_unittest.cc',
            'frame_editing/frame_editing_unittest.cc',
            'frame_analyzer/video_quality_analysis_unittest.cc',
          ],
          # Disable warnings to enable Win64 build, issue 1323.
          'msvs_disabled_warnings': [
            4267,  # size_t to int truncation.
          ],
          'conditions': [
            ['OS=="android"', {
              'dependencies': [
                '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
              ],
            }],
          ],
        }, # tools_unittests
        {
          'target_name': 'rtp_analyzer',
          'type': 'none',
          'variables': {
            'copy_output_dir%': '<(PRODUCT_DIR)',
          },
          'copies': [
            {
              'destination': '<(copy_output_dir)/',
              'files': [
                'py_event_log_analyzer/misc.py',
                'py_event_log_analyzer/pb_parse.py',
                'py_event_log_analyzer/rtp_analyzer.py',
                'py_event_log_analyzer/rtp_analyzer.sh',
              ]
            },
          ],
          'dependencies': [ '<(webrtc_root)/webrtc.gyp:rtc_event_log_proto' ],
          'process_outputs_as_sources': 1,
        }, # rtp_analyzer
      ], # targets
      'conditions': [
        ['OS=="android"', {
          'targets': [
            {
              'target_name': 'tools_unittests_apk_target',
              'type': 'none',
              'dependencies': [
                '<(android_tests_path):tools_unittests_apk',
              ],
            },
          ],
          'conditions': [
            ['test_isolation_mode != "noop"',
              {
                'targets': [
                  {
                    'target_name': 'tools_unittests_apk_run',
                    'type': 'none',
                    'dependencies': [
                      '<(android_tests_path):tools_unittests_apk',
                    ],
                    'includes': [
                      '../build/isolate.gypi',
                    ],
                    'sources': [
                      'tools_unittests_apk.isolate',
                    ],
                  },
                ],
              },
            ],
          ],
        }],
        ['test_isolation_mode != "noop"', {
          'targets': [
            {
              'target_name': 'tools_unittests_run',
              'type': 'none',
              'dependencies': [
                'tools_unittests',
              ],
              'includes': [
                '../build/isolate.gypi',
              ],
              'sources': [
                'tools_unittests.isolate',
              ],
            },
          ],
        }],
      ],
    }], # include_tests
  ], # conditions
}
