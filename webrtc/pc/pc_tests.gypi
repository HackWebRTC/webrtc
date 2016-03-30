# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': ['../build/common.gypi'],
  'targets': [
    {
      'target_name': 'rtc_pc_unittests',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/api/api.gyp:libjingle_peerconnection',
        '<(webrtc_root)/base/base_tests.gyp:rtc_base_tests_utils',
        '<(webrtc_root)/webrtc.gyp:rtc_unittest_main',
        '<(webrtc_root)/pc/pc.gyp:rtc_pc',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/libsrtp/srtp',
      ],
      'sources': [
        'bundlefilter_unittest.cc',
        'channel_unittest.cc',
        'channelmanager_unittest.cc',
        'currentspeakermonitor_unittest.cc',
        'mediasession_unittest.cc',
        'rtcpmuxfilter_unittest.cc',
        'srtpfilter_unittest.cc',
      ],
      # TODO(kjellander): Make the code compile without disabling these flags.
      # See https://bugs.chromium.org/p/webrtc/issues/detail?id=3307
      'cflags_cc!': [
        '-Wnon-virtual-dtor',
      ],
      'conditions': [
        ['clang==0', {
          'cflags': [
            '-Wno-maybe-uninitialized',  # Only exists for GCC.
          ],
        }],
        ['build_libsrtp==1', {
          'dependencies': [
            '<(DEPTH)/third_party/libsrtp/libsrtp.gyp:libsrtp',
          ],
        }],
        ['OS=="win"', {
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalDependencies': [
                'strmiids.lib',
              ],
            },
          },
        }],
      ],
    },  # target rtc_pc_unittests
  ],
}
