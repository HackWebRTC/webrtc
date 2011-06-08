# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This is included for external code which may require different warning
# settings than the internal WebRTC code.

# TODO(ajm): remove these when possible.
{
  'conditions': [
    ['OS=="linux"', {
      'target_defaults': {
        'cflags!': [
          '-Wall',
          '-Wextra',
          '-Werror',
        ],
      },
    }],
    ['OS=="win"', {
      'target_defaults': {
        'defines': [
          '_CRT_SECURE_NO_DEPRECATE',
          '_CRT_NONSTDC_NO_WARNINGS',
          '_CRT_NONSTDC_NO_DEPRECATE',
          '_SCL_SECURE_NO_DEPRECATE',
        ],
        'msvs_disabled_warnings': [4800],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'WarnAsError': 'false',
            'Detect64BitPortabilityProblems': 'false',
          },
        },
      },
    }],
    ['OS=="mac"', {
      'target_defaults': {
        'xcode_settings': {
          'GCC_TREAT_WARNINGS_AS_ERRORS': 'NO',
          'WARNING_CFLAGS!': ['-Wall'],
        },
      },
    }],
  ],
}
