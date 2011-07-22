# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This file contains common settings for building WebRTC components.

{
  'variables': {
    # Putting a variables dict inside another variables dict looks kind of
    # weird.  This is done so that 'build_with_chromium' is defined as
    # variable within the outer variables dict here.  This is necessary
    # to get these variables defined for the conditions within this variables
    # dict that operate on these variables (e.g., for setting
    # 'include_pulse_audio', we need to have 'build_with_chromium' already set).
    'variables': {
      # TODO(ajm): use webrtc_standalone to match NaCl?
      'build_with_chromium%': 1, # 1 to build webrtc with chromium
    },

    'build_with_chromium%': '<(build_with_chromium)',

    # Selects fixed-point code where possible.
    # TODO(ajm): we'd like to set this based on the target OS/architecture.
    'prefer_fixed_point%': 0,

    'conditions': [
      ['OS=="win"', {
        # TODO(ajm, perkj): does this need to be here?
        # Path needed to build Direct Show base classes on Windows.
        # The code is included in the Windows SDK.
        'direct_show_base_classes':
          'C:/Program Files/Microsoft SDKs/Windows/v7.1/Samples/multimedia/directshow/baseclasses/',
      }],
      ['build_with_chromium==1', {
        # Exclude pulse audio on Chromium since its prerequisites don't
        # include pulse audio.
        'include_pulse_audio%': 0,
      }, {
        'include_pulse_audio%': 1,
      }],
    ], # conditions
  },
  'target_defaults': {
    'include_dirs': [
      '..','../..', # common_types.h, typedefs.h
    ],
    'conditions': [
      ['OS=="linux"', {
        'defines': [
          'WEBRTC_TARGET_PC',
          'WEBRTC_LINUX',
          'WEBRTC_THREAD_RR',
          # TODO(ajm): can we select this automatically?
          # Define this if the Linux system does not support CLOCK_MONOTONIC.
          #'WEBRTC_CLOCK_TYPE_REALTIME',
        ],
      }],
      ['OS=="mac"', {
        # TODO(ajm): what about PowerPC?
        # Setup for Intel
        'defines': [
          'WEBRTC_TARGET_MAC_INTEL',
          'WEBRTC_MAC_INTEL',
          'WEBRTC_MAC',
          'WEBRTC_THREAD_RR',
          'WEBRTC_CLOCK_TYPE_REALTIME',
        ],
      }],
      ['OS=="win"', {
        'defines': [
          'WEBRTC_TARGET_PC',
         ],
      }],
      ['build_with_chromium==1', {
        'defines': [
          'WEBRTC_VIDEO_EXTERNAL_CAPTURE_AND_RENDER',
        ],
      }],
    ], # conditions
  }, # target_defaults
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
