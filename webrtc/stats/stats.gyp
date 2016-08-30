# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [ '../build/common.gypi', ],
  'targets': [
    {
      # GN version: webrtc/stats:rtc_stats
      'target_name': 'rtc_stats',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/api/api.gyp:libjingle_peerconnection',
      ],
      'sources': [
        'rtcstats.cc',
        'rtcstats_objects.cc',
        'rtcstatscollector.cc',
        'rtcstatscollector.h',
        'rtcstatsreport.cc',
      ],
    },
  ],
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          # GN version: webrtc/stats:rtc_stats_unittests
          'target_name': 'rtc_stats_unittests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            '<(DEPTH)/testing/gmock.gyp:gmock',
            '<(webrtc_root)/base/base_tests.gyp:rtc_base_tests_utils',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:metrics_default',
            'rtc_stats',
          ],
          'sources': [
            'rtcstats_unittest.cc',
            'rtcstatscollector_unittest.cc',
            'rtcstatsreport_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
