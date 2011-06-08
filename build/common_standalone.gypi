# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This is included when gyp_webrtc is run, and provides the settings
# necessary for a standalone WebRTC build.

{
  'variables': {
    # .gyp files or targets should set webrtc_code to 1 if they build
    # WebRTC-specific code, as opposed to external code.  This variable is
    # used to control such things as the set of warnings to enable, and
    # whether warnings are treated as errors.
    'webrtc_code%': 0,

    'variables': {
      # Compute the architecture that we're building on.
      'conditions': [
        ['OS=="win" or OS=="mac"', {
          'host_arch%': 'ia32',
        }, {
          # This handles the Unix platforms for which there is some support.
          # Anything else gets passed through, which probably won't work very
          # well; such hosts should pass an explicit target_arch to gyp.
          'host_arch%':
            '<!(uname -m | sed -e "s/i.86/ia32/;s/x86_64/x64/;s/amd64/x64/;s/arm.*/arm/;s/i86pc/ia32/")',
        }],

        # A flag for POSIX platforms
        ['OS=="win"', {
          'os_posix%': 0,
        }, {
          'os_posix%': 1,
        }],
      ], # conditions

      # Workaround for libjpeg_turbo pulled from Chromium.
      'chromeos%': 0,
    },

    # Copy conditionally-set variables out one scope.
    'host_arch%': '<(host_arch)',
    'chromeos%': '<(chromeos)',
    'os_posix%': '<(os_posix)',

    # Workaround for GTest pulled from Chromium.
    # TODO(ajm): would be nice to support Clang though...
    #
    # Set this to true when building with Clang.
    # See http://code.google.com/p/chromium/wiki/Clang for details.
    # TODO: eventually clang should behave identically to gcc, and this
    # won't be necessary.
    'clang%': 0,

    # Default architecture we're building for is the architecture we're
    # building on.
    'target_arch%': '<(host_arch)',

    'library%': 'static_library',
  },

  'target_defaults': {
    'include_dirs': [
      '..', # common_types.h, typedefs.h
    ],
    'conditions': [
      ['OS=="linux"', {
        'cflags': [
          '-Wall',
          '-Wextra',
          # TODO(ajm): enable when possible.
          #'-Werror',
        ],
      }],
    ], # conditions
  }, # target_defaults

  'conditions': [
    ['webrtc_code==0', {
      # This section must follow the other conditon sections above because
      # external_code.gypi expects to be merged into those settings.
      'includes': [
        'external_code.gypi',
      ],
    }],
  ], # conditions
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
