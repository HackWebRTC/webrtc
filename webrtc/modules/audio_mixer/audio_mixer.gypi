# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'new_audio_conference_mixer',
      'type': 'static_library',
      'dependencies': [
        'audio_processing',
        'webrtc_utility',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
        '<(webrtc_root)/base/base.gyp:rtc_base_approved',
        '<(webrtc_root)/voice_engine/voice_engine.gyp:level_indicator',
      ],
      'sources': [
        'audio_frame_manipulator.cc',
        'audio_frame_manipulator.h',
        'new_audio_conference_mixer.h',
        'audio_mixer_defines.h',
        'new_audio_conference_mixer_impl.cc',
        'new_audio_conference_mixer_impl.h',
      ],
    },
    {
      'target_name': 'audio_mixer',
      'type': 'static_library',
      'dependencies': [
        'new_audio_conference_mixer',
        'webrtc_utility',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
      'sources': [
        'audio_mixer.h',
        'audio_mixer.cc',
      ],
    },
  ], # targets
}
