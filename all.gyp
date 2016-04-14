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
  },
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        'webrtc/webrtc.gyp:*',
      ],
      'conditions': [
        ['include_examples==1', {
          'dependencies': [
            'webrtc/webrtc_examples.gyp:*',
          ],
        }],
        ['OS=="ios" or (OS=="mac" and target_arch!="ia32")', {
          'dependencies': [
            'talk/app/webrtc/legacy_objc_api.gyp:*',
          ],
          'conditions': [
            ['include_tests==1', {
              'dependencies': [
                'talk/app/webrtc/legacy_objc_api_tests.gyp:*',
              ],
            }],
          ],
        }],
      ],
    },
  ],
}
