# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../../../../../common_settings.gypi', # Common settings
  ],
  'targets': [
    {
      'target_name': 'agc',
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
        '../interface/gain_control.h',
        'analog_agc.c',
        'analog_agc.h',
        'digital_agc.c',
        'digital_agc.h',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
