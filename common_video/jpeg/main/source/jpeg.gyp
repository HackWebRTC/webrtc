# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../../../../common_settings.gypi', # Common settings
  ],
  'targets': [
    {
      'target_name': 'webrtc_jpeg',
      'type': '<(library)',
      'dependencies': [
        '../../../vplib/main/source/vplib.gyp:webrtc_vplib',
      ],
      'include_dirs': [
        '../interface',
        '../../../../../../',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
        ],
      },
      'conditions': [
        ['build_with_chromium==1', {
          'dependencies': [
            '../../../../../libjpeg_turbo/libjpeg.gyp:libjpeg',
          ],
		  'include_dirs': [
            '../../../../../libjpeg_turbo',
          ],
		  'direct_dependent_settings': {
			'include_dirs': [
			  '../../../../../libjpeg_turbo', 
			],
		  },
        },{
          'dependencies': [
            '../../../../../third_party/libjpeg_turbo/libjpeg.gyp:libjpeg',
          ],
          'include_dirs': [
            '../../../../third_party/libjpeg_turbo',
          ],
          'direct_dependent_settings': {
			'include_dirs': [
			  '../../../../third_party/libjpeg_turbo', 
			],
		  },
        }],
      ],
      'sources': [
        # interfaces
        '../interface/jpeg.h',
       
        # headers
        'data_manager.h',

        # sources
        'jpeg.cc',
        'data_manager.cc',
      ],
    },
    {
      'target_name': 'jpeg_test',
      'type': 'executable',
      'dependencies': [
         'webrtc_jpeg',
      ],
      'include_dirs': [
         '../interface',
         '../../../vplib/main/interface',
         '../source',
      ],
      'sources': [

        # headers
        '../test/test_buffer.h',
        
        
        # sources
        '../test/test_buffer.cc',
        '../test/test_jpeg.cc',

      ], # source
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
