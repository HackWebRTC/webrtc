# Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../../build/common.gypi',
    'codecs/interfaces.gypi',
    'codecs/cng/cng.gypi',
    'codecs/g711/g711.gypi',
    'codecs/g722/g722.gypi',
    'codecs/ilbc/ilbc.gypi',
    'codecs/isac/main/source/isac.gypi',
    'codecs/isac/fix/source/isacfix.gypi',
    'codecs/pcm16b/pcm16b.gypi',
    'codecs/red/red.gypi',
    'main/acm2/audio_coding_module.gypi',
    'neteq/neteq.gypi',
  ],
  'conditions': [
    ['include_opus==1', {
      'includes': ['codecs/opus/opus.gypi',],
    }],
    ['include_tests==1', {
      'includes': [
        'codecs/isac/isac_test.gypi',
        'codecs/isac/isacfix_test.gypi',
      ],
      'targets': [
        {
          'target_name': 'audio_codec_speed_tests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            'audio_processing',
            'iSACFix',
            'webrtc_opus',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/test/test.gyp:test_support_main',
          ],
          'sources': [
            'codecs/isac/fix/test/isac_speed_test.cc',
            'codecs/opus/opus_speed_test.cc',
            'codecs/tools/audio_codec_speed_test.h',
            'codecs/tools/audio_codec_speed_test.cc',
          ],
          'conditions': [
            ['OS=="android"', {
              'dependencies': [
                '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
              ],
            }],
          ],
        },
      ],
    }],
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'audio_codec_speed_tests_apk_target',
          'type': 'none',
          'dependencies': [
            '<(apk_tests_path):audio_codec_speed_tests_apk',
          ],
        },
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'audio_codec_speed_tests_run',
          'type': 'none',
          'dependencies': [
            'audio_codec_speed_tests',
          ],
          'includes': [
            '../../build/isolate.gypi',
          ],
          'sources': [
            'audio_codec_speed_tests.isolate',
          ],
        },
      ],
    }],
  ],
}
