# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    'common_settings.gypi',
  ],
  'targets': [
    # Auto test - command line test for all platforms
    {
      'target_name': 'voe_auto_test',
      'type': 'executable',
      'dependencies': [
        'voice_engine/main/source/voice_engine_core.gyp:voice_engine_core',
        'system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        'voice_engine/main/test/auto_test',
      ],
      'sources': [
        'voice_engine/main/test/auto_test/voe_cpu_test.cc',
        'voice_engine/main/test/auto_test/voe_cpu_test.h',
        'voice_engine/main/test/auto_test/voe_extended_test.cc',
        'voice_engine/main/test/auto_test/voe_extended_test.h',
        'voice_engine/main/test/auto_test/voe_standard_test.cc',
        'voice_engine/main/test/auto_test/voe_standard_test.h',
        'voice_engine/main/test/auto_test/voe_stress_test.cc',
        'voice_engine/main/test/auto_test/voe_stress_test.h',
        'voice_engine/main/test/auto_test/voe_test_defines.h',
        'voice_engine/main/test/auto_test/voe_test_interface.h',
        'voice_engine/main/test/auto_test/voe_unit_test.cc',
        'voice_engine/main/test/auto_test/voe_unit_test.h',
      ],
      'conditions': [
        ['OS=="linux" or OS=="mac"', {
          'actions': [
            {
              'action_name': 'copy audio file',
              'inputs': [
                'voice_engine/main/test/auto_test/audio_long16.pcm',
              ],
              'outputs': [
                '/tmp/audio_long16.pcm',
              ],
              'action': [
                '/bin/sh', '-c',
                'cp -f voice_engine/main/test/auto_test/audio_* /tmp/;'\
                'cp -f voice_engine/main/test/auto_test/audio_short16.pcm /tmp/;',
              ],
            },
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            'voice_engine.gyp:voe_ui_win_test',
          ],
        }],
        ['OS=="win"', { 
          'actions': [
            {
              'action_name': 'copy audio file',
              'inputs': [
                'voice_engine/main/test/auto_test/audio_long16.pcm',
              ],
              'outputs': [
                '/tmp/audio_long16.pcm',
              ],
              'action': [
                'cmd', '/c',
                'xcopy /Y /R .\\voice_engine\\main\\test\\auto_test\\audio_* \\tmp',
              ],
            },
            {
              'action_name': 'copy audio audio_short16.pcm',
              'inputs': [
                'voice_engine/main/test/auto_test/audio_short16.pcm',
              ],
              'outputs': [
                '/tmp/audio_short16.pcm',
              ],
              'action': [
                'cmd', '/c',    
                'xcopy /Y /R .\\voice_engine\\main\\test\\auto_test\\audio_short16.pcm \\tmp',
              ],
            },
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        # WinTest - GUI test for Windows
        {
          'target_name': 'voe_ui_win_test',
          'type': 'executable',
          'dependencies': [
            'voice_engine/main/source/voice_engine_core.gyp:voice_engine_core',
            'system_wrappers/source/system_wrappers.gyp:system_wrappers',
          ],
          'include_dirs': [
            'voice_engine/main/test/win_test',
          ],
          'sources': [
            'voice_engine/main/test/win_test/Resource.h',
            'voice_engine/main/test/win_test/WinTest.cpp',
            'voice_engine/main/test/win_test/WinTest.h',
            'voice_engine/main/test/win_test/WinTest.rc',
            'voice_engine/main/test/win_test/WinTestDlg.cpp',
            'voice_engine/main/test/win_test/WinTestDlg.h',
            'voice_engine/main/test/win_test/res/WinTest.ico',
            'voice_engine/main/test/win_test/res/WinTest.rc2',
            'voice_engine/main/test/win_test/stdafx.cpp',
            'voice_engine/main/test/win_test/stdafx.h',
          ],
          'actions': [
            {
              'action_name': 'copy audio file',
              'inputs': [
                'voice_engine/main/test/win_test/audio_tiny11.wav',
              ],
              'outputs': [
                '/tmp/audio_tiny11.wav',
              ],
              'action': [
                'cmd', '/c',
                'xcopy /Y /R .\\voice_engine\\main\\test\\win_test\\audio_* \\tmp',
              ],
            },
            {
              'action_name': 'copy audio audio_short16.pcm',
              'inputs': [
                'voice_engine/main/test/win_test/audio_short16.pcm',
              ],
              'outputs': [
                '/tmp/audio_short16.pcm',
              ],
              'action': [
                'cmd', '/c',    
                'xcopy /Y /R .\\voice_engine\\main\\test\\win_test\\audio_short16.pcm \\tmp',
              ],
            },
            {
              'action_name': 'copy audio_long16noise.pcm',
              'inputs': [
                'voice_engine/main/test/win_test/saudio_long16noise.pcm',
              ],
              'outputs': [
                '/tmp/audio_long16noise.pcm',
              ],
              'action': [
                'cmd', '/c',    
                'xcopy /Y /R .\\voice_engine\\main\\test\\win_test\\audio_long16noise.pcm \\tmp',
              ],
            },
          ],
          'configurations': {
            'Common_Base': {
              'msvs_configuration_attributes': {
                'UseOfMFC': '1',  # Static
              },
            },
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '2',   # Windows
            },
          },
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
