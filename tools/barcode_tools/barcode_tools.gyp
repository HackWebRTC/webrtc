# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'zxing',
      'message': 'build zxing barcode tool',
      'type': 'none',
      'actions': [
        {
          'action_name': 'build_zxing_core',
          'inputs': [
            '<(DEPTH)/third_party/zxing/core/build.xml',
          ],
          'outputs': [
            '<(DEPTH)/third_party/zxing/core/core.jar',
          ],
          'action': [
            'ant',
            '-buildfile',
            '<(DEPTH)/third_party/zxing/core/build.xml',
          ]
        },
        {
          'action_name': 'build_zxing_javase',
          'inputs': [
            '<(DEPTH)/third_party/zxing/javase/build.xml',
          ],
          'outputs': [
            '<(DEPTH)/third_party/zxing/javase/javase.jar',
          ],
          'action': [
            'ant',
            '-buildfile',
            '<(DEPTH)/third_party/zxing/javase/build.xml',
          ]
        },
      ],
    },
  ],
}
