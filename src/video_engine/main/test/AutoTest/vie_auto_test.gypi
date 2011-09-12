# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'variables': {
    'autotest_name': 'vie_auto_test',
  },
  'targets': [
    {
      'target_name': 'merged_lib',
      'type': 'none',
      'dependencies': [
        '<(autotest_name)',
      ],
      'actions': [
        {
          'variables': {
            'output_lib_name': 'webrtc',
            'output_lib': '<(PRODUCT_DIR)/<(STATIC_LIB_PREFIX)<(output_lib_name)_<(OS)<(STATIC_LIB_SUFFIX)',
          },
          'action_name': 'merge_libs',
          'inputs': ['<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)<(autotest_name)<(EXECUTABLE_SUFFIX)'],
          'outputs': ['<(output_lib)'],
          'action': ['python',
                     '../build/merge_libs.py',
                     '<(PRODUCT_DIR)',
                     '<(output_lib)'],
        },
      ],
    },
    {
      'target_name': '<(autotest_name)',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '<(webrtc_root)/modules/modules.gyp:video_render_module',
        '<(webrtc_root)/modules/modules.gyp:video_capture_module',
        '<(webrtc_root)/voice_engine/voice_engine.gyp:voice_engine_core',
        'video_engine_core',        
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
            'vie_win_test',
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
                'cp -f main/test/AutoTest/media/* /tmp/',
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
                'xcopy /Y /R main\\test\\AutoTest\\media\\* \\tmp',
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
