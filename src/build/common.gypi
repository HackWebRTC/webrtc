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
      'build_with_chromium%': 1, # 1 to build webrtc with chromium
    },

    'build_with_chromium%': '<(build_with_chromium)',

    # The Chromium common.gypi we use treats all gyp files without
    # chromium_code==1 as third party code. This disables many of the
    # preferred warning settings.
    #
    # We can set this here to have WebRTC code treated as Chromium code. In a
    # standalone build, our third party code will still have the reduced
    # warning settings.
    'chromium_code': 1,

    # Adds video support to dependencies shared by voice and video engine.
    # This should normally be enabled; the intended use is to disable only
    # when building voice engine exclusively.
    'enable_video%': 1,

    # Selects fixed-point code where possible.
    # TODO(andrew): we'd like to set this based on the target OS/architecture.
    'prefer_fixed_point%': 0,

    # Enable data logging. Produces text files with data logged within engines
    # which can be easily parsed for offline processing.
    'enable_data_logging%': 0,

    'conditions': [
      ['OS=="win"', {
        # TODO(andrew, perkj): does this need to be here?
        # Path needed to build Direct Show base classes on Windows.
        # The code is included in the Windows SDK.
        'direct_show_base_classes':
          'C:/Program Files/Microsoft SDKs/Windows/v7.1/Samples/multimedia/directshow/baseclasses/',
      }],
      ['build_with_chromium==1', {
        # Exclude pulse audio on Chromium since its prerequisites don't require
        # pulse audio.
        'include_pulse_audio%': 0,

        # Exclude internal ADM since Chromium uses its own IO handling.
        'include_internal_audio_device%': 0,
        
        # Exclude internal VCM on Chromium build
        'include_internal_video_capture%': 0,
        
        # Exclude internal video render module on Chromium build
        'include_internal_video_render%': 0,

        'webrtc_root%': '<(DEPTH)/third_party/webrtc',
      }, {
        # Settings for the standalone (not-in-Chromium) build.
        'include_pulse_audio%': 1,

        'include_internal_audio_device%': 1,
        
        'include_internal_video_capture%': 1,
        
        'include_internal_video_render%': 1,

        'webrtc_root%': '<(DEPTH)/src',
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
          # TODO(andrew): can we select this automatically?
          # Define this if the Linux system does not support CLOCK_MONOTONIC.
          #'WEBRTC_CLOCK_TYPE_REALTIME',
        ],
      }],
      ['OS=="mac"', {
        # TODO(andrew): what about PowerPC?
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
    ], # conditions

    'target_conditions': [
      # TODO(andrew): This block disables some warnings from the chromium_code
      # configuration. Remove when possible.
      ['OS=="mac"', {
        'xcode_settings': {
          'GCC_TREAT_WARNINGS_AS_ERRORS': 'NO',
        },
      }],
      ['OS=="win"', {
        'msvs_disabled_warnings': [4389], # Signed/unsigned mismatch.
        'msvs_settings': {
          'VCCLCompilerTool': {
            'WarnAsError': 'false',
          },
        },
      }],
    ], # target_conditions
  }, # target_defaults
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
