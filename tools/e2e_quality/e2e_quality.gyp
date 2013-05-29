# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': ['../../webrtc/build/common.gypi'],
  'targets': [
    {
      'target_name': 'audio_e2e_harness',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/test/channel_transport.gyp:channel_transport',
        '<(webrtc_root)/voice_engine/voice_engine.gyp:voice_engine',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/google-gflags/google-gflags.gyp:google-gflags',
      ],
      'sources': [
        'audio/audio_e2e_harness.cc',
      ],
    },
  ],
}
