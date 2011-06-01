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
      'target_name': 'aec',
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
        '../interface/echo_cancellation.h',
        'echo_cancellation.c',
        'aec_core.c',
        'aec_core_sse2.c',
        'aec_core.h',
        'resampler.c',
        'resampler.h',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
