# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../../../../../../common_settings.gypi', # Common settings
  ],
  'targets': [
    {
      'target_name': 'G711',
      'type': '<(library)',
      'include_dirs': [
        '../interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
        ],
      },
      'sources': [
       '../interface/g711_interface.h',
        'g711_interface.c',
        'g711.c',
        'g711.h',
      ],
    },

      {
      'target_name': 'g711_test',
      'type': 'executable',
      'dependencies': [
        'G711',
      ],
      'sources': [
           '../testG711/testG711.cpp',
      ],
 #     'conditions': [
 #       ['OS=="linux"', {
 #         'cflags': [
 #           '-fexceptions', # enable exceptions
 #         ],
 #       }],
 #     ],
    },
      ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
