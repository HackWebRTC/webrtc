# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../../../../common_settings.gypi',
  ],
  'targets': [
    {
      'target_name': 'audio_device',
      'type': '<(library)',
      'dependencies': [
        '../../../../common_audio/resampler/main/source/resampler.gyp:resampler',
        '../../../../common_audio/signal_processing_library/main/source/spl.gyp:spl',
        '../../../../system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        '.',
        '../../../interface',
        '../interface',
        'Dummy', # Dummy audio device
        'Linux', # Dummy audio device uses linux utility (empty)
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../../../../',
          '../../../interface',
          '../interface',
        ],
      },
      # TODO(xians): Rename files to e.g. *_linux.{ext}, remove sources in conditions section
      'sources': [
        '../interface/audio_device.h',
        '../interface/audio_device_defines.h',
        'audio_device_buffer.cc',
        'audio_device_buffer.h',
        'audio_device_generic.cc',
        'audio_device_generic.h',
        'audio_device_utility.cc',
        'audio_device_utility.h',
        'audio_device_impl.cc',
        'audio_device_impl.h',
        'audio_device_config.h',
        'Dummy/audio_device_dummy.cc',
        'Dummy/audio_device_dummy.h',
        'Linux/alsasymboltable.cc',
        'Linux/alsasymboltable.h',
        'Linux/audio_device_linux_alsa.cc',
        'Linux/audio_device_linux_alsa.h',
        'Linux/audio_device_utility_linux.cc',
        'Linux/audio_device_utility_linux.h',
        'Linux/audio_mixer_manager_linux_alsa.cc',
        'Linux/audio_mixer_manager_linux_alsa.h',
        'Linux/latebindingsymboltable.cc',
        'Linux/latebindingsymboltable.h',
        'Mac/audio_device_mac.cc',
        'Mac/audio_device_mac.h',
        'Mac/audio_device_utility_mac.cc',
        'Mac/audio_device_utility_mac.h',
        'Mac/audio_mixer_manager_mac.cc',
        'Mac/audio_mixer_manager_mac.h',
        'Mac/portaudio/pa_memorybarrier.h',
        'Mac/portaudio/pa_ringbuffer.c',
        'Mac/portaudio/pa_ringbuffer.h',
        'Windows/audio_device_utility_windows.cc',
        'Windows/audio_device_utility_windows.h',
        'Windows/audio_device_windows_core.cc',
        'Windows/audio_device_windows_core.h',
        'Windows/audio_device_windows_wave.cc',
        'Windows/audio_device_windows_wave.h',
        'Windows/audio_mixer_manager.cc',
        'Windows/audio_mixer_manager.h',
      ],
      'conditions': [
        ['OS!="linux"', {
          'sources!': [
            'Linux/alsasymboltable.cc',
            'Linux/alsasymboltable.h',
            'Linux/audio_device_linux_alsa.cc',
            'Linux/audio_device_linux_alsa.h',
            'Linux/audio_mixer_manager_linux_alsa.cc',
            'Linux/audio_mixer_manager_linux_alsa.h',
            'Linux/latebindingsymboltable.cc',
            'Linux/latebindingsymboltable.h',
            # Don't remove these, needed for dummy device
            # 'Linux/audio_device_utility_linux.cc',
            # 'Linux/audio_device_utility_linux.h',
          ],
        }],
        ['OS!="mac"', {
          'sources!': [
            'Mac/audio_device_mac.cc',
            'Mac/audio_device_mac.h',
            'Mac/audio_device_utility_mac.cc',
            'Mac/audio_device_utility_mac.h',
            'Mac/audio_mixer_manager_mac.cc',
            'Mac/audio_mixer_manager_mac.h',
            'Mac/portaudio/pa_memorybarrier.h',
            'Mac/portaudio/pa_ringbuffer.c',
            'Mac/portaudio/pa_ringbuffer.h',
          ],
        }],
        ['OS!="win"', {
          'sources!': [
            'Windows/audio_device_utility_windows.cc',
            'Windows/audio_device_utility_windows.h',
            'Windows/audio_device_windows_core.cc',
            'Windows/audio_device_windows_core.h',
            'Windows/audio_device_windows_wave.cc',
            'Windows/audio_device_windows_wave.h',
            'Windows/audio_mixer_manager.cc',
            'Windows/audio_mixer_manager.h',
          ],
        }],
        ['OS=="linux"', {
          'defines': [
            'LINUX_ALSA',
          ],
          'include_dirs': [
            'Linux',
          ],
          'link_settings': {
            'libraries': [
              '-ldl',
              '-lasound',
            ],
          },
          'conditions': [
            ['include_pulse_audio==1', {
              'defines': [
                'LINUX_PULSE',
              ],
              'sources': [
                'Linux/audio_device_linux_pulse.cc',
                'Linux/audio_device_linux_pulse.h',
                'Linux/audio_mixer_manager_linux_pulse.cc',
                'Linux/audio_mixer_manager_linux_pulse.h',
                'Linux/pulseaudiosymboltable.cc',
                'Linux/pulseaudiosymboltable.h',
              ],
              'link_settings': {
                'libraries': [
                  '-lpulse',
                ],
              },
            }],
          ],
        }],
        ['OS=="mac"', {
          'include_dirs': [
            'Mac',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AudioToolbox.framework',
              '$(SDKROOT)/System/Library/Frameworks/CoreAudio.framework',
            ],
          },
        }],
        ['OS=="win"', {
          'include_dirs': [
            'Windows',
            '../../../../../..',
          ],
        }],
	    ] # conditions
    },
    {
      'target_name': 'audio_device_test_api',
      'type': 'executable',
      'dependencies': [
        'audio_device',
        '../../../../system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '../../../utility/source/utility.gyp:webrtc_utility',
      ],
      'sources': [
        '../test/audio_device_test_api.cc',
        '../test/audio_device_test_defines.h',
      ],
    },
    {
      'target_name': 'audio_device_test_func',
      'type': 'executable',
      'dependencies': [
        'audio_device',
        '../../../../common_audio/resampler/main/source/resampler.gyp:resampler',
        '../../../../system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '../../../utility/source/utility.gyp:webrtc_utility',
      ],
      'sources': [
        '../test/audio_device_test_func.cc',
        '../test/audio_device_test_defines.h',
        '../test/func_test_manager.cc',
        '../test/func_test_manager.h',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
