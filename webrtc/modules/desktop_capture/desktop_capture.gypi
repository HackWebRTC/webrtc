# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'variables': {
    'conditions': [
      # Desktop capturer is supported only on Windows, OSX and Linux.
      ['OS=="win" or OS=="mac" or OS=="linux"', {
        'desktop_capture_enabled%': 1,
      }, {
        'desktop_capture_enabled%': 0,
      }],
    ],
  },
  'conditions': [
    ['desktop_capture_enabled==1', {
      'targets': [
        {
          'target_name': 'desktop_capture',
          'type': 'static_library',
          'dependencies': [
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
          ],
          'direct_dependent_settings': {
            # Headers may use include path relative to webrtc root and depend on
            # WEBRTC_WIN define, so we need to make sure dependent targets have
            # these settings.
            #
            # TODO(sergeyu): Move these settings to common.gypi
            'include_dirs': [
              '../../..',
            ],
            'conditions': [
              ['OS=="win"', {
                'defines': [
                  'WEBRTC_WIN',
                ],
              }],
            ],
          },
          'sources': [
            "desktop_capturer.h",
            "desktop_frame.cc",
            "desktop_frame.h",
            "desktop_frame_win.cc",
            "desktop_frame_win.h",
            "desktop_geometry.cc",
            "desktop_geometry.h",
            "desktop_region.cc",
            "desktop_region.h",
            "shared_memory.cc",
            "shared_memory.h",
            "window_capturer.h",
            "window_capturer_linux.cc",
            "window_capturer_mac.cc",
            "window_capturer_win.cc",
          ],
        },
      ],  # targets
    }],  # desktop_capture_enabled==1
    ['desktop_capture_enabled==1 and include_tests==1', {
      'targets': [
        {
          'target_name': 'desktop_capture_unittests',
          'type': 'executable',
          'dependencies': [
            'desktop_capture',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/test/test.gyp:test_support',
            '<(webrtc_root)/test/test.gyp:test_support_main',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'sources': [
            "window_capturer_unittest.cc",
          ],
        },
      ], # targets
    }],  # desktop_capture_enabled==1 && include_tests==1
  ],
}
