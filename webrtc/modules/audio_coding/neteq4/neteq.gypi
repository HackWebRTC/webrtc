# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'variables': {
    'neteq_dependencies': [
      'G711',
      'G722',
      'PCM16B',
      'iLBC',
      'iSAC',
      'iSACFix',
      'CNG',
      '<(webrtc_root)/common_audio/common_audio.gyp:signal_processing',
      '<(webrtc_root)/common_audio/common_audio.gyp:vad',
      '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
    ],
    'neteq_defines': [],
    'conditions': [
      ['include_opus==1', {
        'neteq_dependencies': ['webrtc_opus',],
        'neteq_defines': ['WEBRTC_CODEC_OPUS',],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'NetEq4',
      'type': 'static_library',
      'dependencies': [
        '<@(neteq_dependencies)',
      ],
      'defines': [
        '<@(neteq_defines)',
      ],
      'include_dirs': [
        'interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'interface',
        ],
      },
      'sources': [
        'interface/audio_decoder.h',
        'interface/neteq.h',
        'accelerate.cc',
        'accelerate.h',
        'audio_decoder_impl.cc',
        'audio_decoder_impl.h',
        'audio_decoder.cc',
        'audio_multi_vector.cc',
        'audio_multi_vector.h',
        'audio_vector.cc',
        'audio_vector.h',
        'background_noise.cc',
        'background_noise.h',
        'buffer_level_filter.cc',
        'buffer_level_filter.h',
        'comfort_noise.cc',
        'comfort_noise.h',
        'decision_logic.cc',
        'decision_logic.h',
        'decision_logic_fax.cc',
        'decision_logic_fax.h',
        'decision_logic_normal.cc',
        'decision_logic_normal.h',
        'decoder_database.cc',
        'decoder_database.h',
        'defines.h',
        'delay_manager.cc',
        'delay_manager.h',
        'delay_peak_detector.cc',
        'delay_peak_detector.h',
        'dsp_helper.cc',
        'dsp_helper.h',
        'dtmf_buffer.cc',
        'dtmf_buffer.h',
        'dtmf_tone_generator.cc',
        'dtmf_tone_generator.h',
        'expand.cc',
        'expand.h',
        'merge.cc',
        'merge.h',
        'neteq_impl.cc',
        'neteq_impl.h',
        'neteq.cc',
        'statistics_calculator.cc',
        'statistics_calculator.h',
        'normal.cc',
        'normal.h',
        'packet_buffer.cc',
        'packet_buffer.h',
        'payload_splitter.cc',
        'payload_splitter.h',
        'post_decode_vad.cc',
        'post_decode_vad.h',
        'preemptive_expand.cc',
        'preemptive_expand.h',
        'random_vector.cc',
        'random_vector.h',
        'rtcp.cc',
        'rtcp.h',
        'sync_buffer.cc',
        'sync_buffer.h',
        'timestamp_scaler.cc',
        'timestamp_scaler.h',
        'time_stretch.cc',
        'time_stretch.h',
      ],
    },
  ], # targets
  'conditions': [
    ['include_tests==1', {
      'includes': ['neteq_tests.gypi',],
      'targets': [
        {
          'target_name': 'neteq4_unittests',
          'type': 'executable',
          'dependencies': [
            'NetEq4',
            'NetEq4TestTools',
            'neteq_unittest_tools',
            'PCM16B',
            '<(DEPTH)/testing/gmock.gyp:gmock',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(webrtc_root)/test/test.gyp:test_support_main',
          ],
          'sources': [
            'audio_multi_vector_unittest.cc',
            'audio_vector_unittest.cc',
            'background_noise_unittest.cc',
            'buffer_level_filter_unittest.cc',
            'comfort_noise_unittest.cc',
            'decision_logic_unittest.cc',
            'decoder_database_unittest.cc',
            'delay_manager_unittest.cc',
            'delay_peak_detector_unittest.cc',
            'dsp_helper_unittest.cc',
            'dtmf_buffer_unittest.cc',
            'dtmf_tone_generator_unittest.cc',
            'expand_unittest.cc',
            'merge_unittest.cc',
            'neteq_external_decoder_unittest.cc',
            'neteq_impl_unittest.cc',
            'neteq_stereo_unittest.cc',
            'neteq_unittest.cc',
            'normal_unittest.cc',
            'packet_buffer_unittest.cc',
            'payload_splitter_unittest.cc',
            'post_decode_vad_unittest.cc',
            'random_vector_unittest.cc',
            'sync_buffer_unittest.cc',
            'timestamp_scaler_unittest.cc',
            'time_stretch_unittest.cc',
            'mock/mock_audio_decoder.h',
            'mock/mock_audio_vector.h',
            'mock/mock_buffer_level_filter.h',
            'mock/mock_decoder_database.h',
            'mock/mock_delay_manager.h',
            'mock/mock_delay_peak_detector.h',
            'mock/mock_dtmf_buffer.h',
            'mock/mock_dtmf_tone_generator.h',
            'mock/mock_external_decoder_pcm16b.h',
            'mock/mock_packet_buffer.h',
            'mock/mock_payload_splitter.h',
          ],
        }, # neteq_unittests

        {
          'target_name': 'audio_decoder_unittests',
          'type': 'executable',
          'dependencies': [
            '<@(neteq_dependencies)',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(webrtc_root)/common_audio/common_audio.gyp:resampler',
            '<(webrtc_root)/test/test.gyp:test_support_main',
          ],
          'defines': [
            'AUDIO_DECODER_UNITTEST',
            'WEBRTC_CODEC_G722',
            'WEBRTC_CODEC_ILBC',
            'WEBRTC_CODEC_ISACFX',
            'WEBRTC_CODEC_ISAC',
            'WEBRTC_CODEC_PCM16',
            '<@(neteq_defines)',
          ],
          'sources': [
            'audio_decoder_impl.cc',
            'audio_decoder_impl.h',
            'audio_decoder_unittest.cc',
            'audio_decoder.cc',
            'interface/audio_decoder.h',
          ],
        }, # audio_decoder_unittest

        {
          'target_name': 'neteq_unittest_tools',
          'type': 'static_library',
          'dependencies': [
            '<(DEPTH)/testing/gmock.gyp:gmock',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(webrtc_root)/test/test.gyp:test_support_main',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'tools',
            ],
          },
          'include_dirs': [
            'tools',
          ],
          'sources': [
            'tools/input_audio_file.cc',
            'tools/input_audio_file.h',
            'tools/rtp_generator.cc',
            'tools/rtp_generator.h',
          ],
        }, # neteq_unittest_tools
      ], # targets
    }], # include_tests
  ], # conditions
}
