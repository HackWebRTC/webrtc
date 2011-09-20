# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'rtp_format_vp8_unittest',
      'type': 'executable',
      'dependencies': [
        'rtp_rtcp',
        '../../testing/gtest.gyp:gtest',
        '../../testing/gtest.gyp:gtest_main',
      ],
      'include_dirs': [
        '.',
      ],
      'sources': [
        'rtp_format_vp8_unittest.cc',
      ],
    },
    {
      'target_name': 'rtcp_format_remb_unittest',
      'type': 'executable',
      'dependencies': [
        'rtp_rtcp',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '../../testing/gtest.gyp:gtest',
        '../../testing/gtest.gyp:gtest_main',
      ],
      'include_dirs': [
        '.',
        '../../../',
      ],
      'sources': [
        'rtcp_format_remb_unittest.cc',
      ],
    },
    {
      'target_name': 'rtp_utility_test',
      'type': 'executable',
      'dependencies': [
        'rtp_rtcp',
        '../../testing/gtest.gyp:gtest',
        '../../testing/gtest.gyp:gtest_main',
      ],
      'include_dirs': [
        '.',
      ],
      'sources': [
        'rtp_utility_test.cc',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
