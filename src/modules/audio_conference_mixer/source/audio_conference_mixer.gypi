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
      'target_name': 'audio_conference_mixer',
      'type': '<(library)',
      'dependencies': [
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
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
        '../interface/audio_conference_mixer.h',
        '../interface/audio_conference_mixer_defines.h',
        'audio_frame_manipulator.cc',
        'audio_frame_manipulator.h',
        'level_indicator.cc',
        'level_indicator.h',
        'memory_pool.h',
        'memory_pool_generic.h',
        'memory_pool_windows.h',
        'audio_conference_mixer_impl.cc',
        'audio_conference_mixer_impl.h',
        'time_scheduler.cc',
        'time_scheduler.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources!': [
            'memory_pool_generic.h',
          ],
        }],
        ['OS!="win"', {
          'sources!': [
            'memory_pool_windows.h',
          ],
        }],
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
