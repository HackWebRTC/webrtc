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
      'target_name': 'apm_util',
      'type': '<(library)',
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
      'sources': [
        'ring_buffer.c',
        'ring_buffer.h',
        'fft4g.c',
        'fft4g.h',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
