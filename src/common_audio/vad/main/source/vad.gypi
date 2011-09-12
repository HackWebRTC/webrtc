# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'vad',
      'type': '<(library)',
      'dependencies': [
        'spl',
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
        '../interface/webrtc_vad.h',
        'webrtc_vad.c',
        'vad_const.c',
        'vad_const.h',
        'vad_defines.h',
        'vad_core.c',
        'vad_core.h',
        'vad_filterbank.c',
        'vad_filterbank.h',
        'vad_gmm.c',
        'vad_gmm.h',
        'vad_sp.c',
        'vad_sp.h',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
