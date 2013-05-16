# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': ['webrtc/build/common.gypi',],
  'variables': {
    'webrtc_all_dependencies': [
      'webrtc/common_audio/common_audio.gyp:*',
      'webrtc/common_video/common_video.gyp:*',
      'webrtc/modules/modules.gyp:*',
      'webrtc/system_wrappers/source/system_wrappers.gyp:*',
      'webrtc/video_engine/video_engine.gyp:*',
      'webrtc/voice_engine/voice_engine.gyp:*',
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
            'webrtc/system_wrappers/source/system_wrappers_tests.gyp:*',
            'webrtc/test/channel_transport.gyp:*',
            'webrtc/test/metrics.gyp:*',
            'webrtc/test/test.gyp:*',
            'webrtc/tools/tools.gyp:*',
            'webrtc/tools/force_mic_volume_max.gyp:*',
            'tools/e2e_quality/e2e_quality.gyp:*',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'common_unittests',
          'type': 'executable',
          'dependencies': [
             '<(DEPTH)/testing/gtest.gyp:gtest',
             '<(webrtc_root)/test/test.gyp:test_support_main',
          ],
          'sources': [
            'webrtc/common_unittest.cc',
          ],
        },
      ],  # targets
    }],  # include_tests
  ],
}
