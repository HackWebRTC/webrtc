# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'variables': {
    'use_libjpeg_turbo%': '<(use_libjpeg_turbo)',
  },
  'targets': [
    {
      'target_name': 'webrtc_jpeg',
      'type': '<(library)',
      'dependencies': [
        'webrtc_libyuv',
      ],
      'include_dirs': [
        '../../../interface',
        '../interface',
        '../../../../../../',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
          '../../../interface',
        ],
      },
      'conditions': [
        ['build_libjpeg==1', {
          'conditions': [
            ['build_with_chromium==1', {
              'dependencies': [
                '<(libjpeg_gyp_path):libjpeg',
              ],
            }, {
              'conditions': [
                ['use_libjpeg_turbo==1', {
                  'dependencies': [
                    '<(DEPTH)/third_party/libjpeg_turbo/libjpeg.gyp:libjpeg',
                  ],
                }, {
                  'dependencies': [
                    '<(DEPTH)/third_party/libjpeg/libjpeg.gyp:libjpeg',
                  ],
                }],
              ],
            }],
          ],
        }],
      ],
      'sources': [
        '../interface/jpeg.h',
        'data_manager.cc',
        'data_manager.h',
        'jpeg.cc',
      ],
    },
  ], # targets
  # Exclude the test target when building with chromium.
  'conditions': [
    ['build_with_chromium==0', {
      'targets': [
        {
          'target_name': 'jpeg_test',
          'type': 'executable',
          'dependencies': [
             'webrtc_jpeg',
          ],
          'include_dirs': [
            '../interface',
            '../source',
          ],
          'sources': [
            '../test/test_jpeg.cc',
          ],
        },
      ] # targets
    }], # build_with_chromium
  ], # conditions
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
