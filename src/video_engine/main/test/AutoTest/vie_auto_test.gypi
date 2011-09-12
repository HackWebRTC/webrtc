# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../common_settings.gypi', # Common settings
  ],
  'targets': [
    {
      'target_name': '<(autotest_name)',
      'type': 'executable',
      'dependencies': [
        'system_wrappers/source/system_wrappers.gyp:system_wrappers',
        'modules/video_render/main/source/video_render.gyp:video_render_module',
        'modules/video_capture/main/source/video_capture.gyp:video_capture_module',
        'voice_engine/main/source/voice_engine_core.gyp:voice_engine_core',
        'video_engine/main/source/video_engine_core.gyp:video_engine_core',        
      ],
      'include_dirs': [
        'interface/',
        '../../interface',
        '../../source',
        '../../../../modules/video_capture/main/source/',
        '../../../../modules/video_coding/codecs/interface/',
        '../../../../common_video/interface/',
      ],
      'sources': [
        # interfaces
        'interface/tb_capture_device.h',
        'interface/tb_external_transport.h',
        'interface/tb_I420_codec.h',
        'interface/tb_interfaces.h',
        'interface/tb_video_channel.h',
        'interface/vie_autotest.h',
        'interface/vie_autotest_defines.h',
        'interface/vie_autotest_linux.h',
        'interface/vie_autotest_mac_carbon.h',
        'interface/vie_autotest_mac_cocoa.h',
        'interface/vie_autotest_main.h',
        'interface/vie_autotest_window_manager_interface.h',
        'interface/vie_autotest_windows.h',

        # PLATFORM INDEPENDENT SOURCE FILES
        'source/tb_capture_device.cc',
        'source/tb_external_transport.cc',
        'source/tb_I420_codec.cc',
        'source/tb_interfaces.cc',
        'source/tb_video_channel.cc',
        'source/vie_autotest.cc',
        'source/vie_autotest_base.cc',
        'source/vie_autotest_capture.cc',
        'source/vie_autotest_codec.cc',
        'source/vie_autotest_encryption.cc',
        'source/vie_autotest_file.cc',
        'source/vie_autotest_image_process.cc',
        'source/vie_autotest_loopback.cc',
        'source/vie_autotest_main.cc',
        'source/vie_autotest_network.cc',
        'source/vie_autotest_render.cc',
        'source/vie_autotest_rtp_rtcp.cc',
        'source/vie_autotest_custom_call.cc',
        # PLATFORM SPECIFIC SOURCE FILES - Will be filtered below
        # Linux
        'source/vie_autotest_linux.cc',
        # Mac
        'source/vie_autotest_mac_cocoa.cc',
        'source/vie_autotest_mac_carbon.cc',
        # Windows
        'source/vie_autotest_windows.cc',
      ], # sources
      'conditions': [
        # DEFINE PLATFORM SPECIFIC SOURCE FILES
        ['OS!="linux"', {
          'sources!': [
            'source/vie_autotest_linux.cc',
          ],
        }],
        ['OS!="mac"', {
          'sources!': [
            'source/vie_autotest_mac_cocoa.cc',
            'source/vie_autotest_mac_carbon.cc',
          ],
        }],
        ['OS!="win"', {
          'sources!': [
            'source/vie_autotest_windows.cc',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [            
            'video_engine/main/test/WindowsTest/windowstest.gyp:vie_win_test',
          ],
        }],
        
       # DEFINE PLATFORM SPECIFIC INCLUDE AND CFLAGS
        ['OS=="mac" or OS=="linux"', {
          'cflags': [
            '-Wno-write-strings',
          ],
          'ldflags': [
            '-lpthread -lm',
          ],
        }],
        ['OS=="linux"', {
          'ldflags': [
          #  '-L<(libvpx_hack_dir)/<(OS)/<(target_arch)',
          ],
          'libraries': [
            '-lrt',
            '-lXext',
            '-lX11',
            '-lasound',
            '-lpulse',


          ],
        }],
        ['OS=="mac"', {
          'xcode_settings': {
            'OTHER_CPLUSPLUSFLAGS': '-x objective-c++',
            'OTHER_LDFLAGS': [
              '-framework Foundation -framework AppKit -framework Cocoa -framework OpenGL -framework CoreVideo -framework CoreAudio -framework AudioToolbox',
            ],
          },
        }],
      #Copy media files
        ['OS=="linux" or OS=="mac"', {
          'actions': [
            {
              'action_name': 'copy media files',
              'inputs': [
                'media',
              ],
              'outputs': [
                'captureDeviceImage.bmp',
              ],
              'action': [
                '/bin/sh', '-c',
                'cp -f video_engine/main/test/AutoTest/media/* /tmp/',
              ],
            },
          ],
        }],
        ['OS=="win"', {
          'actions': [
             {
              'action_name': 'copy media files',
              'inputs': [
                'media',
              ],
              'outputs': [
                '\\tmp\\*.jpg',
                '\\tmp\\*.bmp',
              ],
              'action': [
                'cmd', '/c',
                'xcopy /Y /R video_engine\\main\\test\\AutoTest\\media\\* \\tmp',
              ],
            },
          ],
        }],
      ], #conditions  
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
