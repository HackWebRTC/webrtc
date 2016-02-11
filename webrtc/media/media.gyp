# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [ '../build/common.gypi', ],
  'targets': [
    {
      'target_name': 'rtc_media',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/base/base.gyp:rtc_base_approved',
        '<(webrtc_root)/common.gyp:webrtc_common',
        '<(webrtc_root)/modules/modules.gyp:video_render_module',
        '<(webrtc_root)/webrtc.gyp:webrtc',
        '<(webrtc_root)/voice_engine/voice_engine.gyp:voice_engine',
        '<(webrtc_root)/sound/sound.gyp:rtc_sound',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:metrics_default',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
        '<(webrtc_root)/libjingle/xmllite/xmllite.gyp:rtc_xmllite',
        '<(webrtc_root)/libjingle/xmpp/xmpp.gyp:rtc_xmpp',
        '<(webrtc_root)/p2p/p2p.gyp:rtc_p2p',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(libyuv_dir)/include',
        ],
      },
      'sources': [
        'base/audioframe.h',
        'base/audiorenderer.h',
        'base/capturemanager.cc',
        'base/capturemanager.h',
        'base/capturerenderadapter.cc',
        'base/capturerenderadapter.h',
        'base/codec.cc',
        'base/codec.h',
        'base/constants.cc',
        'base/constants.h',
        'base/cpuid.cc',
        'base/cpuid.h',
        'base/cryptoparams.h',
        'base/device.h',
        'base/fakescreencapturerfactory.h',
        'base/hybriddataengine.h',
        'base/mediachannel.h',
        'base/mediacommon.h',
        'base/mediaengine.cc',
        'base/mediaengine.h',
        'base/rtpdataengine.cc',
        'base/rtpdataengine.h',
        'base/rtpdump.cc',
        'base/rtpdump.h',
        'base/rtputils.cc',
        'base/rtputils.h',
        'base/screencastid.h',
        'base/streamparams.cc',
        'base/streamparams.h',
        'base/turnutils.cc',
        'base/turnutils.h',
        'base/videoadapter.cc',
        'base/videoadapter.h',
        'base/videocapturer.cc',
        'base/videocapturer.h',
        'base/videocapturerfactory.h',
        'base/videocommon.cc',
        'base/videocommon.h',
        'base/videoframe.cc',
        'base/videoframe.h',
        'base/videoframefactory.cc',
        'base/videoframefactory.h',
        'base/videorenderer.h',
        'base/yuvframegenerator.cc',
        'base/yuvframegenerator.h',
        'devices/deviceinfo.h',
        'devices/devicemanager.cc',
        'devices/devicemanager.h',
        'devices/dummydevicemanager.h',
        'devices/videorendererfactory.h',
        'sctp/sctpdataengine.cc',
        'sctp/sctpdataengine.h',
        'webrtc/nullwebrtcvideoengine.h',
        'webrtc/simulcast.cc',
        'webrtc/simulcast.h',
        'webrtc/webrtccommon.h',
        'webrtc/webrtcmediaengine.cc',
        'webrtc/webrtcmediaengine.h',
        'webrtc/webrtcmediaengine.cc',
        'webrtc/webrtcvideocapturer.cc',
        'webrtc/webrtcvideocapturer.h',
        'webrtc/webrtcvideocapturerfactory.h',
        'webrtc/webrtcvideocapturerfactory.cc',
        'webrtc/webrtcvideodecoderfactory.h',
        'webrtc/webrtcvideoencoderfactory.h',
        'webrtc/webrtcvideoengine2.cc',
        'webrtc/webrtcvideoengine2.h',
        'webrtc/webrtcvideoframe.cc',
        'webrtc/webrtcvideoframe.h',
        'webrtc/webrtcvideoframefactory.cc',
        'webrtc/webrtcvideoframefactory.h',
        'webrtc/webrtcvoe.h',
        'webrtc/webrtcvoiceengine.cc',
        'webrtc/webrtcvoiceengine.h',
      ],
      # TODO(kjellander): Make the code compile without disabling these flags.
      # See https://bugs.chromium.org/p/webrtc/issues/detail?id=3307
      'cflags': [
        '-Wno-deprecated-declarations',
      ],
      'cflags!': [
        '-Wextra',
      ],
      'cflags_cc!': [
        '-Wnon-virtual-dtor',
        '-Woverloaded-virtual',
      ],
      'msvs_disabled_warnings': [
        4245,  # conversion from 'int' to 'size_t', signed/unsigned mismatch.
        4267,  # conversion from 'size_t' to 'int', possible loss of data.
        4389,  # signed/unsigned mismatch.
      ],
      'conditions': [
        ['build_libyuv==1', {
          'dependencies': ['<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv',],
        }],
        ['build_usrsctp==1', {
          'include_dirs': [
            # TODO(jiayl): move this into the direct_dependent_settings of
            # usrsctp.gyp.
            '<(DEPTH)/third_party/usrsctp/usrsctplib',
          ],
          'dependencies': [
            '<(DEPTH)/third_party/usrsctp/usrsctp.gyp:usrsctplib',
          ],
        }],
        ['build_with_chromium==1', {
          'dependencies': [
            '<(webrtc_root)/modules/modules.gyp:video_capture',
            '<(webrtc_root)/modules/modules.gyp:video_render',
          ],
        }, {
          'defines': [
            'HAVE_WEBRTC_VIDEO',
            'HAVE_WEBRTC_VOICE',
          ],
          'direct_dependent_settings': {
            'defines': [
              'HAVE_WEBRTC_VIDEO',
              'HAVE_WEBRTC_VOICE',
            ],
          },
          'dependencies': [
            '<(webrtc_root)/modules/modules.gyp:video_capture_module_internal_impl',
            '<(webrtc_root)/modules/modules.gyp:video_render_module_internal_impl',
          ],
        }],
        ['OS=="linux"', {
          'sources': [
            'devices/libudevsymboltable.cc',
            'devices/libudevsymboltable.h',
            'devices/linuxdeviceinfo.cc',
            'devices/linuxdevicemanager.cc',
            'devices/linuxdevicemanager.h',
            'devices/v4llookup.cc',
            'devices/v4llookup.h',
          ],
          'conditions': [
            ['use_gtk==1', {
              'sources': [
                'devices/gtkvideorenderer.cc',
                'devices/gtkvideorenderer.h',
              ],
              'cflags': [
                '<!@(pkg-config --cflags gobject-2.0 gthread-2.0 gtk+-2.0)',
              ],
            }],
          ],
          'include_dirs': [
            'third_party/libudev'
          ],
          'libraries': [
            '-lrt',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            'devices/gdivideorenderer.cc',
            'devices/gdivideorenderer.h',
            'devices/win32deviceinfo.cc',
            'devices/win32devicemanager.cc',
            'devices/win32devicemanager.h',
          ],
          'msvs_settings': {
            'VCLibrarianTool': {
              'AdditionalDependencies': [
                'd3d9.lib',
                'gdi32.lib',
                'strmiids.lib',
                'winmm.lib',
              ],
            },
          },
        }],
        ['OS=="mac"', {
          'sources': [
            'devices/macdeviceinfo.cc',
            'devices/macdevicemanager.cc',
            'devices/macdevicemanager.h',
            'devices/macdevicemanagermm.mm',
          ],
          'conditions': [
            ['target_arch=="ia32"', {
              'sources': [
                'devices/carbonvideorenderer.cc',
                'devices/carbonvideorenderer.h',
              ],
              'link_settings': {
                'xcode_settings': {
                  'OTHER_LDFLAGS': [
                    '-framework Carbon',
                  ],
                },
              },
            }],
          ],
          'xcode_settings': {
            'WARNING_CFLAGS': [
              # TODO(ronghuawu): Update macdevicemanager.cc to stop using
              # deprecated functions and remove this flag.
              '-Wno-deprecated-declarations',
            ],
            # Disable partial availability warning to prevent errors
            # in macdevicemanagermm.mm using AVFoundation.
            # https://code.google.com/p/webrtc/issues/detail?id=4695
            'WARNING_CFLAGS!': ['-Wpartial-availability'],
          },
          'link_settings': {
            'xcode_settings': {
              'OTHER_LDFLAGS': [
                '-weak_framework AVFoundation',
                '-framework Cocoa',
                '-framework CoreAudio',
                '-framework CoreVideo',
                '-framework OpenGL',
                '-framework QTKit',
              ],
            },
          },
        }],
        ['OS=="ios"', {
          'sources': [
            'devices/mobiledevicemanager.cc',
          ],
          'include_dirs': [
            # TODO(sjlee) Remove when vp8 is building for iOS.  vp8 pulls in
            # libjpeg which pulls in libyuv which currently disabled.
            '../../third_party/libyuv/include',
          ],
          # TODO(kjellander): Make the code compile without disabling these.
          # See https://bugs.chromium.org/p/webrtc/issues/detail?id=3307
          'cflags': [
            '-Wno-unused-const-variable',
          ],
          'xcode_settings': {
            'WARNING_CFLAGS': [
              '-Wno-unused-const-variable',
            ],
          },
        }],
        ['OS=="ios" or (OS=="mac" and target_arch!="ia32")', {
          'defines': [
            'CARBON_DEPRECATED=YES',
          ],
        }],
        ['OS=="android"', {
          'sources': [
            'devices/mobiledevicemanager.cc',
          ],
        }],
      ],
    },  # target rtc_media
  ],  # targets.
}
