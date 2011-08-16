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
        'dummy', # dummy audio device
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
        'dummy/audio_device_dummy.cc',
        'dummy/audio_device_dummy.h',
        'dummy/audio_device_utility_dummy.cc',
        'dummy/audio_device_utility_dummy.h',
      ],
      'conditions': [
        ['OS=="linux"', {
          'include_dirs': [
            'linux',
          ],
        }], # OS==linux
        ['OS=="mac"', {
            'include_dirs': [
              'mac',
            ],
        }], # OS==mac
        ['OS=="win"', {
            'include_dirs': [
              'win',
              '../../../../../..',
            ],
        }],
        ['include_internal_audio_device==0', {
          'defines': [
            'WEBRTC_DUMMY_AUDIO_BUILD',
          ],
        }],
        ['include_internal_audio_device==1', {
          'sources': [
            'linux/alsasymboltable_linux.cc',
            'linux/alsasymboltable_linux.h',
            'linux/audio_device_alsa_linux.cc',
            'linux/audio_device_alsa_linux.h',
            'linux/audio_device_utility_linux.cc',
            'linux/audio_device_utility_linux.h',
            'linux/audio_mixer_manager_alsa_linux.cc',
            'linux/audio_mixer_manager_alsa_linux.h',
            'linux/latebindingsymboltable_linux.cc',
            'linux/latebindingsymboltable_linux.h',
            'mac/audio_device_mac.cc',
            'mac/audio_device_mac.h',
            'mac/audio_device_utility_mac.cc',
            'mac/audio_device_utility_mac.h',
            'mac/audio_mixer_manager_mac.cc',
            'mac/audio_mixer_manager_mac.h',
            'mac/portaudio/pa_memorybarrier.h',
            'mac/portaudio/pa_ringbuffer.c',
            'mac/portaudio/pa_ringbuffer.h',
            'win/audio_device_core_win.cc',
            'win/audio_device_core_win.h',
            'win/audio_device_wave_win.cc',
            'win/audio_device_wave_win.h',
            'win/audio_device_utility_win.cc',
            'win/audio_device_utility_win.h',
            'win/audio_mixer_manager_win.cc',
            'win/audio_mixer_manager_win.h',
          ],
          'conditions': [
            ['OS=="linux"', {
              'defines': [
                'LINUX_ALSA',
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
                    'linux/audio_device_pulse_linux.cc',
                    'linux/audio_device_pulse_linux.h',
                    'linux/audio_mixer_manager_pulse_linux.cc',
                    'linux/audio_mixer_manager_pulse_linux.h',
                    'linux/pulseaudiosymboltable_linux.cc',
                    'linux/pulseaudiosymboltable_linux.h',
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
              'link_settings': {
                'libraries': [
                  '$(SDKROOT)/System/Library/Frameworks/AudioToolbox.framework',
                  '$(SDKROOT)/System/Library/Frameworks/CoreAudio.framework',
                ],
              },
            }],
          ], # conditions
        }], # include_internal_audio_device==1
      ], # conditions
    },
  ],
  # Exclude the test targets when building with chromium.
  'conditions': [   
    ['build_with_chromium==0', {
      'targets': [
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
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
