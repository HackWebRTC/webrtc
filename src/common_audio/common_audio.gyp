# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../build/common.gypi',
    'signal_processing_library/main/source/spl.gypi',
    'resampler/main/source/resampler.gypi',
    'vad/main/source/vad.gypi',
  ],
  'conditions': [
    ['build_with_chromium==0', {
      'targets' : [
        {
          'target_name': 'common_audio_unittests',
          'type': 'executable',
          'dependencies': [
            '<(webrtc_root)/../test/test.gyp:test_support',
            '<(webrtc_root)/../testing/gtest.gyp:gtest',
            'resampler',
          ],
          'sources': [
            '<(webrtc_root)/../test/run_all_unittests.cc',
            'resampler/main/source/resampler_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
