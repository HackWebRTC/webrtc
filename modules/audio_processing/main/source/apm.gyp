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
      'target_name': 'audio_processing',
      'type': '<(library)',
      'conditions': [
        ['prefer_fixed_point==1', {
          'dependencies': ['../../ns/main/source/ns.gyp:ns_fix'],
          'defines': ['WEBRTC_NS_FIXED'],
        }, { # else: prefer_fixed_point==0
          'dependencies': ['../../ns/main/source/ns.gyp:ns'],
          'defines': ['WEBRTC_NS_FLOAT'],
        }],
      ],
      'dependencies': [
        '../../../../system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '../../aec/main/source/aec.gyp:aec',
        '../../aecm/main/source/aecm.gyp:aecm',
        '../../agc/main/source/agc.gyp:agc',
        '../../../../common_audio/signal_processing_library/main/source/spl.gyp:spl',
        '../../../../common_audio/vad/main/source/vad.gyp:vad',
      ],
      'include_dirs': [
        '../interface',
        '../../../interface',
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
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
