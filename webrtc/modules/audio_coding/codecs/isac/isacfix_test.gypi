# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    # kenny
    {
      'target_name': 'iSACFixtest',
      'type': 'executable',
      'dependencies': [
        'iSACFix',
        '<(webrtc_root)/test/test.gyp:test_support',
      ],
      'include_dirs': [
        './fix/test',
        './fix/interface',
      ],
      'sources': [
        './fix/test/kenny.cc',
      ],
    },
    {
      'target_name': 'isacfix_unittests',
      'type': 'executable',
      'dependencies': [
        'iSACFix',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(webrtc_root)/test/test.gyp:test_support_main',
      ],
      'sources': [
        'fix/source/filters_unittest.cc',
        'fix/source/filterbanks_unittest.cc',
        'fix/source/lpc_masking_model_unittest.cc',
      ],
    },
  ],
}

# TODO(kma): Add bit-exact test for iSAC-fix.
