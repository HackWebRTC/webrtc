# Copyright (c) 2013 The WebRTC project authors.All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'variables': {
    'xv_renderer%': 0,
  },
  'conditions': [
    ['OS=="linux"', {
      'variables': {
       'glx_renderer%': 1,
      },
    }, {
      # OS != "linux"
      'variables': {
        'glx_renderer%': 0,
      },
    }],
  ],
  'targets': [
    {
      'target_name': 'video_tests_common',
      'type': 'static_library',
      'sources': [
        'common/flags.cc',
        'common/flags.h',
        'common/frame_generator.cc',
        'common/frame_generator.h',
        'common/generate_ssrcs.h',
        'common/vcm_capturer.h',
        'common/vcm_capturer.cc',
        'common/video_capturer.cc',
        'common/video_capturer.h',
        'common/video_renderer.cc',
        'common/video_renderer.h',
      ],
      'conditions': [
        ['glx_renderer==1', {
          'defines': [
            'WEBRTC_TEST_GLX',
          ],
          'sources' : [
            'common/gl/gl_renderer.cc',
            'common/gl/gl_renderer.h',
            'common/linux/glx_renderer.cc',
            'common/linux/glx_renderer.h',
          ],
        }],
        ['xv_renderer==1', {
          'defines': [
            'WEBRTC_TEST_XV',
          ],
          'sources': [
            'common/linux/xv_renderer.cc',
            'common/linux/xv_renderer.h',
          ],
        }],
      ],
      'direct_dependent_settings': {
        'conditions': [
          ['OS=="linux"', {
            'libraries': [
              '-lXext',
              '-lX11',
              '-lGL',
            ],
          }],
          ['xv_renderer==1', {
            'libraries': [
              '-lXv',
            ],
          }],
          #TODO(pbos) : These dependencies should not have to be here, they
          #             aren't used by test code directly, only by components
          #             used by the tests.
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
        '<(DEPTH)/third_party/google-gflags/google-gflags.gyp:google-gflags',
        '<(webrtc_root)/modules/modules.gyp:video_capture_module',
        'video_engine_core',
      ],
    },
    {
      'target_name': 'video_loopback',
      'type': 'executable',
      'sources': [
        'loopback.cc',
      ],
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(webrtc_root)/modules/modules.gyp:video_capture_module',
        'video_tests_common',
      ],
    },
  ],
}
