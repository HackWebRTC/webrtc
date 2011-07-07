# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../common_settings.gypi', # Common settings
  ],
  'targets': [
    {
      'target_name': 'media_file',
      'type': '<(library)',
      'dependencies': [
        '../../utility/source/utility.gyp:webrtc_utility',
        '../../../system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'defines': [
           'WEBRTC_MODULE_UTILITY_VIDEO', # for compiling support for video recording
          ],
      'include_dirs': [
        '../interface',
        '../../interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
          '../../interface',
        ],
      },
      'sources': [
        '../interface/media_file.h',
        '../interface/media_file_defines.h',
        'avi_file.cc',
        'avi_file.h',
        'media_file_impl.cc',
        'media_file_impl.h',
        'media_file_utility.cc',
        'media_file_utility.h',
      ], # source
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
