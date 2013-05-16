# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
{
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'force_mic_volume_max',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/voice_engine/voice_engine.gyp:voice_engine_core',
      ],
      'sources': [
        'force_mic_volume_max/force_mic_volume_max.cc',
      ],
    }, # force_mic_volume_max
  ]
}