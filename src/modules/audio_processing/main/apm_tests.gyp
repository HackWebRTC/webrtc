# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../../../common_settings.gypi',
  ],
  'targets': [
    {
      'target_name': 'unit_test',
      'type': 'executable',
      'conditions': [
        ['prefer_fixed_point==1', {
          'defines': ['WEBRTC_APM_UNIT_TEST_FIXED_PROFILE'],
        }, {
          'defines': ['WEBRTC_APM_UNIT_TEST_FLOAT_PROFILE'],
        }],
      ],
      'dependencies': [
        'source/apm.gyp:audio_processing',
        '../../../system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '../../../common_audio/signal_processing_library/main/source/spl.gyp:spl',

        '../../../../testing/gtest.gyp:gtest',
        '../../../../testing/gtest.gyp:gtest_main',
        '../../../../third_party/protobuf/protobuf.gyp:protobuf_lite',
      ],
      'include_dirs': [
        '../../../../testing/gtest/include',
      ],
      'sources': [
        'test/unit_test/unit_test.cc',
        'test/unit_test/audio_processing_unittest.pb.cc',
        'test/unit_test/audio_processing_unittest.pb.h',
      ],
    },
    {
      'target_name': 'process_test',
      'type': 'executable',
      'dependencies': [
        'source/apm.gyp:audio_processing',
        '../../../system_wrappers/source/system_wrappers.gyp:system_wrappers',

        '../../../../testing/gtest.gyp:gtest',
        '../../../../testing/gtest.gyp:gtest_main',
      ],
      'include_dirs': [
        '../../../../testing/gtest/include',
      ],
      'sources': [
        'test/process_test/process_test.cc',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
