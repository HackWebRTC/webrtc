# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../common_settings.gypi', # Common settings
  ],
  'targets': [
    {
      'target_name': 'video_processing',
      'type': '<(library)',
      'dependencies': [
        '../../../../common_audio/signal_processing_library/main/source/spl.gyp:spl',
        '../../../../common_video/vplib/main/source/vplib.gyp:webrtc_vplib',
        '../../../../system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '../../../utility/source/utility.gyp:webrtc_utility',
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
