# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../common_settings.gypi', # Common settings
  ],
  'targets': [
    {
      'target_name': 'video_coding_test',
      'type': 'executable',
      'dependencies': [
         'video_coding.gyp:webrtc_video_coding',
         '../../../rtp_rtcp/source/rtp_rtcp.gyp:rtp_rtcp',
         '../../../utility/source/utility.gyp:webrtc_utility',
         '../../../video_processing/main/source/video_processing.gyp:video_processing',
         '../../../../common_video/vplib/main/source/vplib.gyp:webrtc_vplib',
      ],
      'include_dirs': [
         '../../../interface',
         '../../codecs/vp8/main/interface',
         '../../../../system_wrappers/interface',
         '../source',
      ],
      'sources': [

        # headers
        '../test/codec_database_test.h',
        '../test/generic_codec_test.h',
        '../test/jitter_estimate_test.h',
        '../test/media_opt_test.h',
        '../test/normal_test.h',
        '../test/quality_modes_test.h',
        '../test/receiver_tests.h',
        '../test/release_test.h',
        '../test/rtp_player.h',
        '../test/test_util.h',
        '../test/video_source.h',

        # sources
        '../test/codec_database_test.cc',
        '../test/decode_from_storage_test.cc',
        '../test/generic_codec_test.cc',
        '../test/jitter_buffer_test.cc',
        '../test/media_opt_test.cc',
        '../test/mt_rx_tx_test.cc',
        '../test/normal_test.cc',
        '../test/quality_modes_test.cc',
        '../test/receiver_timing_tests.cc',
        '../test/rtp_player.cc',
        '../test/test_util.cc',
        '../test/tester_main.cc',
        '../test/video_rtp_play_mt.cc',
        '../test/video_rtp_play.cc',
        '../test/video_source.cc',

      ], # source

      'conditions': [

        ['OS=="linux"', {
          'cflags': [
            '-fexceptions',
          ],
        }],

      ], # conditions
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
