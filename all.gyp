# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'variables': {
    'libjingle_root%': '<(DEPTH)',
    'include_tests%': 1,
    'webrtc_root_additional_dependencies': [
    ],
  },
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        'third_party/openmax_dl/dl/dl.gyp:*',
        'webrtc/webrtc.gyp:*',
        '<(libjingle_root)/talk/libjingle.gyp:*',
        '<(libjingle_root)/talk/libjingle_examples.gyp:*',
        '<@(webrtc_root_additional_dependencies)',
      ],
      'conditions': [
        ['OS=="android"', {
          'dependencies': [
            'webrtc/webrtc_examples.gyp:*',
          ],
        }],
        ['include_tests==1', {
          'dependencies': [
            '<(libjingle_root)/talk/libjingle_tests.gyp:*',
          ],
        }],
      ],
    },
  ],
}
