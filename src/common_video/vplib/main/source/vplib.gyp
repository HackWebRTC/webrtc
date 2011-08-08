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
      'target_name': 'webrtc_vplib',
      'type': '<(library)',
      'dependencies': [
      ],
      'include_dirs': [
        '../interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
        ],
      },
      'sources': [
        # interfaces
        '../interface/vplib.h',
        '../interface/interpolator.h',

        # headers
        'conversion_tables.h',
        'scale_bilinear_yuv.h',
      
        # sources
        'vplib.cc',
        'interpolator.cc',
        'scale_bilinear_yuv.cc',
      ],
    },
  ], # targets
  # Exclude the test target when building with chromium.
  'conditions': [   
    ['build_with_chromium==0', {
      'targets': [
        {
          'target_name': 'vplib_test',
          'type': 'executable',
          'dependencies': [
            'webrtc_vplib',
          ],
          'include_dirs': [
             '../interface',
             '../source',
          ],
          'sources': [

            # headers
            '../test/test_util.h',
        
            # sources
            '../test/tester_main.cc',
            '../test/scale_test.cc',
            '../test/convert_test.cc',
            '../test/interpolation_test.cc',
          ], # source
        },
      ], # targets
    }], # build_with_chromium
  ], # conditions
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
