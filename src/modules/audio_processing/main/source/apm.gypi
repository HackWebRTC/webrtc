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
      'target_name': 'audio_processing',
      'type': '<(library)',
      'conditions': [
        ['prefer_fixed_point==1', {
          'dependencies': ['ns_fix'],
          'defines': ['WEBRTC_NS_FIXED'],
        }, {
          'dependencies': ['ns'],
          'defines': ['WEBRTC_NS_FLOAT'],
        }],
        ['build_with_chromium==1', {
          'dependencies': [
            '../../protobuf/protobuf.gyp:protobuf_lite',
          ],
        }, {
          'dependencies': [
            '../../third_party/protobuf/protobuf.gyp:protobuf_lite',
          ],
        }],
      ],
      'dependencies': [
        'debug_proto',
        'aec',
        'aecm',
        'agc',
        '<(webrtc_root)/common_audio/common_audio.gyp:spl',
        '<(webrtc_root)/common_audio/common_audio.gyp:vad',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        '../interface',
        '../../../interface',
        '<(protoc_out_dir)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
          '../../../interface',
        ],
      },
      'sources': [
        '../interface/audio_processing.h',
        'audio_buffer.cc',
        'audio_buffer.h',
        'audio_processing_impl.cc',
        'audio_processing_impl.h',
        'echo_cancellation_impl.cc',
        'echo_cancellation_impl.h',
        'echo_control_mobile_impl.cc',
        'echo_control_mobile_impl.h',
        'gain_control_impl.cc',
        'gain_control_impl.h',
        'high_pass_filter_impl.cc',
        'high_pass_filter_impl.h',
        'level_estimator_impl.cc',
        'level_estimator_impl.h',
        'noise_suppression_impl.cc',
        'noise_suppression_impl.h',
        'splitting_filter.cc',
        'splitting_filter.h',
        'processing_component.cc',
        'processing_component.h',
        'voice_detection_impl.cc',
        'voice_detection_impl.h',
        '<(protoc_out_dir)/<(protoc_out_relpath)/debug.pb.cc',
        '<(protoc_out_dir)/<(protoc_out_relpath)/debug.pb.h',
      ],
    },
    {
      # Protobuf compiler / generate rule for audio_processing
      'target_name': 'debug_proto',
      'type': 'none',
      'variables': {
        'proto_relpath': 'audio_processing/main/source/',
      },
      'sources': [
        '<(proto_relpath)/debug.proto',
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
            '<(protoc_out_dir)/<(protoc_out_relpath)/<(RULE_INPUT_ROOT).pb.h',
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
      'conditions': [
        ['build_with_chromium==1', {
          'dependencies': [
            '../../protobuf/protobuf.gyp:protoc#host',
          ],
        }, {
          'dependencies': [
            '../../third_party/protobuf/protobuf.gyp:protoc#host',
          ],
        }],
      ],
      # This target exports a hard dependency because it generates header
      # files.
      'hard_dependency': 1,
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
