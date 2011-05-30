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
    'build_with_chromium%': 0, # 1 to build webrtc with chromium
    'inside_chromium_build%': 0,

    # Selects fixed-point code where possible.
    # TODO(ajm): we'd like to set this based on the target OS/architecture.
    'prefer_fixed_point%': 0,

    'conditions': [
      ['inside_chromium_build==1', {
        'build_with_chromium': 1,
      }],
      ['OS=="win"', {
        # Path needed to build Direct Show base classes on Windows. The code is included in Windows SDK.
        'direct_show_base_classes':'C:/Program Files/Microsoft SDKs/Windows/v7.1/Samples/multimedia/directshow/baseclasses/',
      }],
    ], # conditions
  },
  'target_defaults': {
    'include_dirs': [
      '.', # For common_typs.h and typedefs.h
    ],
    'conditions': [
      ['OS=="linux"', {
        'defines': [
          'WEBRTC_TARGET_PC',
          'WEBRTC_LINUX',
          'WEBRTC_THREAD_RR',
          # INTEL_OPT is for iLBC floating point code optimized for Intel processors
          # supporting SSE3. The compiler will be automatically switched to Intel 
          # compiler icc in the iLBC folder for iLBC floating point library build.
          #'INTEL_OPT',
          # Define WEBRTC_CLOCK_TYPE_REALTIME if the Linux system does not support CLOCK_MONOTONIC
          #'WEBRTC_CLOCK_TYPE_REALTIME',
        ],
      }],
      ['OS=="mac"', {
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
  }, # target-defaults
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
