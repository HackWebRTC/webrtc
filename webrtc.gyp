# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [ 'src/build/common.gypi', ],
  'variables': {
    'webrtc_all_dependencies': [
      'src/common_audio/common_audio.gyp:*',
      'src/common_video/common_video.gyp:*',
      'src/modules/modules.gyp:*',
      'src/system_wrappers/source/system_wrappers.gyp:*',
      'src/video_engine/video_engine.gyp:*',
      'src/voice_engine/voice_engine.gyp:*',
      '<(webrtc_vp8_dir)/vp8.gyp:*',
    ],
  },
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        '<@(webrtc_all_dependencies)',
      ],
      'conditions': [
        ['include_tests==1', {
          'dependencies': [
            'src/test/metrics.gyp:*',
            'src/test/test.gyp:*',
            'tools/e2e_quality/e2e_quality.gyp:*',
          ],
        }],
      ],
    },
  ],
}
