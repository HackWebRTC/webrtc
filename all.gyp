# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'variables': {
    'talk_root%': '<(DEPTH)/talk',
  },
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        'third_party/openmax_dl/dl/dl.gyp:*',
        'webrtc/webrtc.gyp:*',
        '<(talk_root)/libjingle.gyp:*',
        '<(talk_root)/libjingle_examples.gyp:*',
        '<(talk_root)/libjingle_tests.gyp:*',
      ],
      'conditions': [
        ['OS=="android"', {
          'dependencies': [
            'webrtc/webrtc_examples.gyp:*',
          ],
        }],
      ],
    },
  ],
}
