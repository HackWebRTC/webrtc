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
      'target_name': 'video_processing',
      'type': '<(library)',
      'dependencies': [
        'webrtc_utility',
        '<(webrtc_root)/common_audio/common_audio.gyp:signal_processing',
        '<(webrtc_root)/common_video/common_video.gyp:webrtc_vplib',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
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
        # interfaces
        '../interface/video_processing.h',
        '../interface/video_processing_defines.h',

        # headers
        'video_processing_impl.h',
        'brightness_detection.h',
	'brighten.h',
        'color_enhancement.h',
        'color_enhancement_private.h',
        'content_analysis.h',
        'deflickering.h',
        'denoising.h',
        'frame_preprocessor.h',
        'spatial_resampler.h',
        'video_decimator.h',

        # sources
        'video_processing_impl.cc',
        'brightness_detection.cc',
	'brighten.cc',
        'color_enhancement.cc',
        'content_analysis.cc',
        'deflickering.cc',
        'denoising.cc',
        'frame_preprocessor.cc',
        'spatial_resampler.cc',
        'video_decimator.cc',
      ], # source
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
