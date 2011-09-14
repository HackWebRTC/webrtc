# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# TODO(andrew): consider moving test_support to src/base/test.
{
  'includes': [
    '../src/build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'test_support',
      'type': 'static_library',
      'dependencies': [
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'test_suite.cc',
        'test_suite.h',
      ],
    },
  ],
}
