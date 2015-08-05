# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'variables': {
    'include_examples%': 1,
    'include_tests%': 1,
    'webrtc_root_additional_dependencies': [],
  },
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        'webrtc/webrtc.gyp:*',
        'talk/libjingle.gyp:*',
        '<@(webrtc_root_additional_dependencies)',
      ],
      'conditions': [
        ['include_examples==1', {
          'dependencies': [
            'webrtc/libjingle_examples.gyp:*',
            'webrtc/webrtc_examples.gyp:*',
          ],
        }],
        ['include_tests==1', {
          'dependencies': [
            'talk/libjingle_tests.gyp:*',
          ],
        }],
      ],
    },
  ],
}
