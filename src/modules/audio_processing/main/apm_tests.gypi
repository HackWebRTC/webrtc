# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'variables': {
    'protoc_out_dir': '<(SHARED_INTERMEDIATE_DIR)/protoc_out',
    'protoc_out_relpath': 'webrtc/audio_processing',
  },
  'targets': [
    {
      'target_name': 'audioproc_unittest',
      'type': 'executable',
      'conditions': [
        ['prefer_fixed_point==1', {
          'defines': ['WEBRTC_APM_UNIT_TEST_FIXED_PROFILE'],
        }, {
          'defines': ['WEBRTC_APM_UNIT_TEST_FLOAT_PROFILE'],
        }],
      ],
      'dependencies': [
        'audioproc_unittest_proto',
        'audio_processing',
        '<(webrtc_root)/common_audio/common_audio.gyp:spl',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '<(webrtc_root)/../test/test.gyp:test_support',
        '<(webrtc_root)/../testing/gtest.gyp:gtest',
        '<(webrtc_root)/../testing/gtest.gyp:gtest_main',
        '<(webrtc_root)/../third_party/protobuf/protobuf.gyp:protobuf_lite',
      ],
      'include_dirs': [
        '../../../../testing/gtest/include',
        '<(protoc_out_dir)',
      ],
      'sources': [
        'test/unit_test/unit_test.cc',
        '<(protoc_out_dir)/<(protoc_out_relpath)/unittest.pb.cc',
        '<(protoc_out_dir)/<(protoc_out_relpath)/unittest.pb.h',
      ],
    },
    {
      # Protobuf compiler / generate rule for audioproc_unittest
      'target_name': 'audioproc_unittest_proto',
      'type': 'none',
      'variables': {
        'proto_relpath':
          '<(webrtc_root)/modules/audio_processing/main/test/unit_test',
      },
      'sources': [
        '<(proto_relpath)/unittest.proto',
      ],
      'rules': [
        {
          'rule_name': 'genproto',
          'extension': 'proto',
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
          ],
          'outputs': [
            '<(protoc_out_dir)/<(protoc_out_relpath)/<(RULE_INPUT_ROOT).pb.cc',
            '<(protoc_out_dir)/<(RULE_INPUT_ROOT).pb.h',
          ],
          'action': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
            '--proto_path=<(proto_relpath)',
            '<(proto_relpath)/<(RULE_INPUT_NAME)',
            '--cpp_out=<(protoc_out_dir)/<(protoc_out_relpath)',
          ],
          'message': 'Generating C++ code from <(RULE_INPUT_PATH)',
        },
      ],
      'dependencies': [
        '../../third_party/protobuf/protobuf.gyp:protoc#host',
      ],
      # This target exports a hard dependency because it generates header
      # files.
      'hard_dependency': 1,
    },
    {
      'target_name': 'audioproc_process_test',
      'type': 'executable',
      'dependencies': [
        'audio_processing',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '<(webrtc_root)/../testing/gtest.gyp:gtest',
        '<(webrtc_root)/../testing/gtest.gyp:gtest_main',
        '<(webrtc_root)/../third_party/protobuf/protobuf.gyp:protobuf_lite',
      ],
      'include_dirs': [
        '../../../../testing/gtest/include',
        '<(protoc_out_dir)',
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
