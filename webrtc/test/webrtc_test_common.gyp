# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
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
      'target_name': 'webrtc_test_common',
      'type': 'static_library',
      'sources': [
        'configurable_frame_size_encoder.cc',
        'configurable_frame_size_encoder.h',
        'direct_transport.cc',
        'direct_transport.h',
        'encoder_settings.cc',
        'encoder_settings.h',
        'fake_audio_device.cc',
        'fake_audio_device.h',
        'fake_decoder.cc',
        'fake_decoder.h',
        'fake_encoder.cc',
        'fake_encoder.h',
        'fake_network_pipe.cc',
        'fake_network_pipe.h',
        'flags.cc',
        'flags.h',
        'frame_generator_capturer.cc',
        'frame_generator_capturer.h',
        'mock_transport.h',
        'null_platform_renderer.cc',
        'null_transport.cc',
        'null_transport.h',
        'rtp_rtcp_observer.h',
        'run_tests.cc',
        'run_tests.h',
        'run_loop.cc',
        'run_loop.h',
        'statistics.cc',
        'statistics.h',
        'vcm_capturer.cc',
        'vcm_capturer.h',
        'video_capturer.cc',
        'video_capturer.h',
        'video_renderer.cc',
        'video_renderer.h',
      ],
      # TODO(pbos): As far as I can tell these are dependencies from
      # video_render and they should really not be here. This target provides
      # no platform-specific rendering.
      'direct_dependent_settings': {
        'conditions': [
          ['OS=="linux"', {
            'libraries': [
              '-lXext',
              '-lX11',
              '-lGL',
            ],
          }],
          ['OS=="android"', {
            'libraries' : [
              '-lGLESv2', '-llog',
            ],
          }],
          ['OS=="mac"', {
            'xcode_settings' : {
              'OTHER_LDFLAGS' : [
                '-framework Foundation',
                '-framework AppKit',
                '-framework Cocoa',
                '-framework OpenGL',
                '-framework CoreVideo',
                '-framework CoreAudio',
                '-framework AudioToolbox',
              ],
            },
          }],
        ],
      },
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
        '<(webrtc_root)/modules/modules.gyp:video_capture_module',
        '<(webrtc_root)/modules/modules.gyp:media_file',
        '<(webrtc_root)/test/test.gyp:frame_generator',
        '<(webrtc_root)/test/test.gyp:test_support',
      ],
    },
  ],
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'webrtc_test_common_unittests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            'webrtc_test_common',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(DEPTH)/testing/gmock.gyp:gmock',
            '<(webrtc_root)/test/test.gyp:test_support_main',
          ],
          'sources': [
            'fake_network_pipe_unittest.cc',
          ],
        },
      ],  #targets
    }],  # include_tests
  ],  # conditions
}
