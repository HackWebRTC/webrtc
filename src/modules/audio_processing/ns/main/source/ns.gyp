# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../../../../../common_settings.gypi',
  ],
  'targets': [
    {
      'target_name': 'ns',
      'type': '<(library)',
      'dependencies': [
        '../../../../../common_audio/signal_processing_library/main/source/spl.gyp:spl',
        '../../../utility/util.gyp:apm_util'
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
        '../interface/noise_suppression.h',
        'noise_suppression.c',
        'windows_private.h',
        'defines.h',
        'ns_core.c',
        'ns_core.h',
      ],
    },
    {
      'target_name': 'ns_fix',
      'type': '<(library)',
      'dependencies': [
        '../../../../../common_audio/signal_processing_library/main/source/spl.gyp:spl',
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
        '../interface/noise_suppression_x.h',
        'noise_suppression_x.c',
        'nsx_defines.h',
        'nsx_core.c',
        'nsx_core.h',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
