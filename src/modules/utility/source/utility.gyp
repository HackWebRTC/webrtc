# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../common_settings.gypi', # Common settings
  ],
  'targets': [
    {
      'target_name': 'webrtc_utility',
      'type': '<(library)',
      'dependencies': [
        '../../audio_coding/main/source/audio_coding_module.gyp:audio_coding_module',
        '../../../common_audio/resampler/main/source/resampler.gyp:resampler',
        '../../../system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        '../interface',
        '../../interface',
        '../../media_file/interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
          '../../interface',
          '../../audio_coding/main/interface',
        ],
      },
      'sources': [
        '../interface/file_player.h',
        '../interface/file_recorder.h',
        '../interface/process_thread.h',
        '../interface/rtp_dump.h',
        'coder.cc',
        'coder.h',
        'file_player_impl.cc',
        'file_player_impl.h',
        'file_recorder_impl.cc',
        'file_recorder_impl.h',
        'process_thread_impl.cc',
        'process_thread_impl.h',
        'rtp_dump_impl.cc',
        'rtp_dump_impl.h',
      ],
      'conditions': [
        ['enable_video==1', {
          # Adds support for video recording.
          'defines': [
            'WEBRTC_MODULE_UTILITY_VIDEO',
          ],
          'dependencies': [
            '../../video_coding/main/source/video_coding.gyp:webrtc_video_coding',
          ],
          'include_dirs': [
            '../../video_coding/main/interface',
            '../../../common_video/vplib/main/interface',
          ],
          'sources': [
            'frame_scaler.cc',
            'video_coder.cc',
            'video_frames_queue.cc',
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
