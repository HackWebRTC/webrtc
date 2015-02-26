# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'video_coding_test',
      'type': 'executable',
      'dependencies': [
         'rtp_rtcp',
         'video_processing',
         'webrtc_video_coding',
         'webrtc_utility',
         '<(DEPTH)/testing/gtest.gyp:gtest',
         '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
         '<(webrtc_root)/common.gyp:webrtc_common',
         '<(webrtc_root)/test/test.gyp:test_support',
         '<(webrtc_root)/test/metrics.gyp:metrics',
         '<(webrtc_root)/common_video/common_video.gyp:common_video',
         '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers_default',
         '<(webrtc_root)/test/webrtc_test_common.gyp:webrtc_test_common',
      ],
      'sources': [
        # headers
        'main/test/codec_database_test.h',
        'main/test/generic_codec_test.h',
        'main/test/media_opt_test.h',
        'main/test/mt_test_common.h',
        'main/test/normal_test.h',
        'main/test/quality_modes_test.h',
        'main/test/receiver_tests.h',
        'main/test/release_test.h',
        'main/test/rtp_player.h',
        'main/test/test_callbacks.h',
        'main/test/test_util.h',
        'main/test/vcm_payload_sink_factory.h',
        'main/test/video_source.h',

        # sources
        'main/test/codec_database_test.cc',
        'main/test/generic_codec_test.cc',
        'main/test/media_opt_test.cc',
        'main/test/mt_rx_tx_test.cc',
        'main/test/mt_test_common.cc',
        'main/test/normal_test.cc',
        'main/test/quality_modes_test.cc',
        'main/test/rtp_player.cc',
        'main/test/test_callbacks.cc',
        'main/test/test_util.cc',
        'main/test/tester_main.cc',
        'main/test/vcm_payload_sink_factory.cc',
        'main/test/video_rtp_play.cc',
        'main/test/video_rtp_play_mt.cc',
        'main/test/video_source.cc',
      ], # sources
    },
  ],
}
