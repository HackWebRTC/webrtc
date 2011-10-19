# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'video_capture_module',
      'type': '<(library)',
      'dependencies': [
        'webrtc_utility',
        '<(webrtc_root)/common_video/common_video.gyp:webrtc_vplib',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        '../interface',
        '../../../interface',
        '../../../../common_video/vplib/main/interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
          '../../../interface',
          '../../../../common_video/vplib/main/interface',
        ],
      },
      'sources': [
        # interfaces
        '../interface/video_capture.h',
        '../interface/video_capture_defines.h',
        '../interface/video_capture_factory.h',
        # headers
        'video_capture_config.h',
        'video_capture_delay.h',
        'video_capture_impl.h',
        'vplib_conversions.h',
        'device_info_impl.h',

        # DEFINE PLATFORM INDEPENDENT SOURCE FILES
        'video_capture_factory.cc',
        'video_capture_impl.cc',
        'vplib_conversions.cc',
        'device_info_impl.cc',
      ],
      'conditions': [
        ['include_internal_video_capture==0', {
          'sources': [
            'External/device_info_external.cc',
            'External/video_capture_external.cc',
          ],
        },{  # include_internal_video_capture == 1
          'conditions': [
            # DEFINE PLATFORM SPECIFIC SOURCE FILES
            ['OS=="linux"', {
              'include_dirs': [
                'Linux',
              ],
              'sources': [
                'Linux/device_info_linux.h',
                'Linux/video_capture_linux.h',
                'Linux/device_info_linux.cc',
                'Linux/video_capture_linux.cc',
              ],
            }],  # linux
            ['OS=="mac"', {
              'sources': [
                'Mac/QTKit/video_capture_recursive_lock.h',
                'Mac/QTKit/video_capture_qtkit.h',
                'Mac/QTKit/video_capture_qtkit_info.h',
                'Mac/QTKit/video_capture_qtkit_info_objc.h',
                'Mac/QTKit/video_capture_qtkit_objc.h',
                'Mac/QTKit/video_capture_qtkit_utility.h',
                'Mac/video_capture_mac.mm',
                'Mac/QTKit/video_capture_qtkit.mm',
                'Mac/QTKit/video_capture_qtkit_objc.mm',
                'Mac/QTKit/video_capture_recursive_lock.mm',
                'Mac/QTKit/video_capture_qtkit_info.mm',
                'Mac/QTKit/video_capture_qtkit_info_objc.mm',
              ],
              'include_dirs': [
                'Mac',
              ],
              'link_settings': {
                'xcode_settings': {
                  'OTHER_LDFLAGS': [
                    '-framework QTKit',
                  ],
                },
              },
            }],  # mac
            ['OS=="win"', {
              'include_dirs': [
                'Windows',
                '<(direct_show_base_classes)',
              ],
              'defines!': [
                'NOMINMAX',
              ],
              'sources': [
                'Windows/help_functions_windows.h',
                'Windows/sink_filter_windows.h',
                'Windows/video_capture_windows.h',
                'Windows/device_info_windows.h',
                'Windows/capture_delay_values_windows.h',
                'Windows/help_functions_windows.cc',
                'Windows/sink_filter_windows.cc',
                'Windows/video_capture_windows.cc',
                'Windows/device_info_windows.cc',
                'Windows/video_capture_factory_windows.cc',
                '<(direct_show_base_classes)amextra.cpp',
                '<(direct_show_base_classes)amextra.h',
                '<(direct_show_base_classes)amfilter.cpp',
                '<(direct_show_base_classes)amfilter.h',
                '<(direct_show_base_classes)amvideo.cpp',
                '<(direct_show_base_classes)arithutil.cpp',
                '<(direct_show_base_classes)cache.h',
                '<(direct_show_base_classes)checkbmi.h',
                '<(direct_show_base_classes)combase.cpp',
                '<(direct_show_base_classes)combase.h',
                '<(direct_show_base_classes)cprop.cpp',
                '<(direct_show_base_classes)cprop.h',
                '<(direct_show_base_classes)ctlutil.cpp',
                '<(direct_show_base_classes)ctlutil.h',
                '<(direct_show_base_classes)ddmm.cpp',
                '<(direct_show_base_classes)ddmm.h',
                '<(direct_show_base_classes)dllentry.cpp',
                '<(direct_show_base_classes)dllsetup.cpp',
                '<(direct_show_base_classes)dllsetup.h',
                '<(direct_show_base_classes)dxmperf.h',
                '<(direct_show_base_classes)fourcc.h',
                '<(direct_show_base_classes)measure.h',
                '<(direct_show_base_classes)msgthrd.h',
                '<(direct_show_base_classes)mtype.cpp',
                '<(direct_show_base_classes)mtype.h',
                '<(direct_show_base_classes)outputq.cpp',
                '<(direct_show_base_classes)outputq.h',
                '<(direct_show_base_classes)perflog.cpp',
                '<(direct_show_base_classes)perflog.h',
                '<(direct_show_base_classes)perfstruct.h',
                '<(direct_show_base_classes)pstream.cpp',
                '<(direct_show_base_classes)pstream.h',
                '<(direct_show_base_classes)pullpin.cpp',
                '<(direct_show_base_classes)pullpin.h',
                '<(direct_show_base_classes)refclock.cpp',
                '<(direct_show_base_classes)refclock.h',
                '<(direct_show_base_classes)reftime.h',
                '<(direct_show_base_classes)renbase.cpp',
                '<(direct_show_base_classes)renbase.h',
                '<(direct_show_base_classes)schedule.cpp',
                '<(direct_show_base_classes)schedule.h',
                '<(direct_show_base_classes)seekpt.cpp',
                '<(direct_show_base_classes)seekpt.h',
                '<(direct_show_base_classes)source.cpp',
                '<(direct_show_base_classes)source.h',
                '<(direct_show_base_classes)streams.h',
                '<(direct_show_base_classes)strmctl.cpp',
                '<(direct_show_base_classes)strmctl.h',
                '<(direct_show_base_classes)sysclock.cpp',
                '<(direct_show_base_classes)sysclock.h',
                '<(direct_show_base_classes)transfrm.cpp',
                '<(direct_show_base_classes)transfrm.h',
                '<(direct_show_base_classes)transip.cpp',
                '<(direct_show_base_classes)transip.h',
                '<(direct_show_base_classes)videoctl.cpp',
                '<(direct_show_base_classes)videoctl.h',
                '<(direct_show_base_classes)vtrans.cpp',
                '<(direct_show_base_classes)vtrans.h',
                '<(direct_show_base_classes)winctrl.cpp',
                '<(direct_show_base_classes)winctrl.h',
                '<(direct_show_base_classes)winutil.cpp',
                '<(direct_show_base_classes)winutil.h',
                '<(direct_show_base_classes)wxdebug.cpp',
                '<(direct_show_base_classes)wxdebug.h',
                '<(direct_show_base_classes)wxlist.cpp',
                '<(direct_show_base_classes)wxlist.h',
                '<(direct_show_base_classes)wxutil.cpp',
                '<(direct_show_base_classes)wxutil.h',
              ],
              'msvs_settings': {
                'VCLibrarianTool': {
                  'AdditionalDependencies': 'Strmiids.lib',
                },
              },
            }],  # win
          ], # conditions
        }],  # include_internal_video_capture
      ], # conditions
    },
  ],
   # Exclude the test targets when building with chromium.
  'conditions': [
    ['build_with_chromium==0', {
      'targets': [
        {
          'target_name': 'video_capture_module_test',
          'type': 'executable',
          'dependencies': [
           'video_capture_module',
           '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
           'webrtc_utility',
           'video_render_module',
           'webrtc_video_coding',
          ],
          'include_dirs': [
            '../interface',
          ],
          'sources': [
            # sources
            '../test/testAPI/cocoa_renderer.h',
            '../test/testAPI/cocoa_renderer.mm',
            '../test/testAPI/testDefines.h',
            '../test/testAPI/testAPI.cpp',
            '../test/testAPI/testCameraEncoder.cpp',
            '../test/testAPI/testCameraEncoder.h',
            '../test/testAPI/testExternalCapture.cpp',
            '../test/testAPI/testExternalCapture.h',
            '../test/testAPI/testPlatformDependent.cpp',
            '../test/testAPI/testPlatformDependent.h',
            '../test/testAPI/Logger.h',
            '../test/testAPI/Logger.cpp',
            '../test/testAPI/Renderer.h',
            '../test/testAPI/Renderer.cpp',
          ], # source
          'conditions': [
            # DEFINE PLATFORM SPECIFIC SOURCE FILES
            ['OS!="mac"', {
              'sources!': [
                '../test/testAPI/cocoa_renderer.h',
                '../test/testAPI/cocoa_renderer.mm',
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
              'libraries': [
                '-lrt',
                '-lXext',
                '-lX11',
              ],
            }],
            ['OS=="mac"', {
              'xcode_settings': {
                # TODO(andrew): remove this. Shouldn't be needed when required
                # files have proper .mm extensions.
                'OTHER_CPLUSPLUSFLAGS': '-x objective-c++',
                'OTHER_LDFLAGS': [
                  '-framework Foundation -framework AppKit -framework Cocoa -framework OpenGL -framework CoreVideo -framework CoreAudio -framework AudioToolbox',
                ],
              },
            }],
          ] # conditions
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
