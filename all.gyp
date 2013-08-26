# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': ['talk/build/common.gypi'],
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        'webrtc/webrtc.gyp:*',
        'talk/libjingle.gyp:*',
        'talk/libjingle_examples.gyp:*',
        'talk/libjingle_tests.gyp:*',
      ],
    },
  ],
}
