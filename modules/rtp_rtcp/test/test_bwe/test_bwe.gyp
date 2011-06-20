# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../../../../common_settings.gypi', # Common settings
  ],
  'targets': [
    {
      'target_name': 'test_bwe',
      'type': 'executable',
      'dependencies': [
        '../../source/rtp_rtcp.gyp:rtp_rtcp',
        '../../../../system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '../../../../../testing/gtest.gyp:gtest',
        '../../../../../testing/gtest.gyp:gtest_main',
      ],
      'include_dirs': [
        '../../source',
      ],
      'sources': [
        'unit_test.cc',
        '../../source/bitrate.cc',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
