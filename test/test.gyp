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
        '<(webrtc_root)/../testing/gtest.gyp:gtest',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
      'sources': [
        'test_suite.cc',
        'test_suite.h',
        'testsupport/fileutils.h',
        'testsupport/fileutils.cc',
      ],
    },
    {
      'target_name': 'test_support_unittests',
      'type': 'executable',
      'dependencies': [
        'test_support',
        '<(webrtc_root)/../testing/gtest.gyp:gtest',
      ],
       'sources': [
        'run_all_unittests.cc',
        'testsupport/fileutils_unittest.cc',
      ],
    },
  ],
}
