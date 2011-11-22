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
      'target_name': 'aec',
      'type': '<(library)',
      'variables': {
        # Outputs some low-level debug files.
        'aec_debug_dump%': 0,
      },
      'dependencies': [
        '<(webrtc_root)/common_audio/common_audio.gyp:signal_processing',
        'apm_util'
      ],
      'include_dirs': [
        'interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'interface',
        ],
      },
      'sources': [
        'interface/echo_cancellation.h',
        'echo_cancellation.c',
        'aec_core.h',
        'aec_core.c',
        'aec_core_sse2.c',
        'aec_rdft.h',
        'aec_rdft.c',
        'aec_rdft_sse2.c',
        'resampler.h',
        'resampler.c',
      ],
      'conditions': [
        ['aec_debug_dump==1', {
          'defines': [ 'WEBRTC_AEC_DEBUG_DUMP', ],
        }],
      ],
    },
  ],
}
