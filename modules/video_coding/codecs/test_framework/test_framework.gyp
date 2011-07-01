# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../common_settings.gypi', # Common settings
  ],
  'targets': [
    {
      'target_name': 'test_framework',
      'type': '<(library)',

      'dependencies': [
        '../../../../system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '../../../../common_video/vplib/main/source/vplib.gyp:webrtc_vplib',
      ],

      'include_dirs': [
        '../interface',
        '../../../../common_video/interface',
      ],

      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
        ],
      },

      'sources': [
        # header files
        'benchmark.h',
        'normal_async_test.h',
        'normal_test.h',
        'packet_loss_test.h',
        'performance_test.h',
        'test.h',
        'unit_test.h',
        'video_buffer.h',
        'video_source.h',

        # source files
        'benchmark.cc',
        'normal_async_test.cc',
        'normal_test.cc',
        'packet_loss_test.cc',
        'performance_test.cc',
        'test.cc',
        'unit_test.cc',
        'video_buffer.cc',
        'video_source.cc',

      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
