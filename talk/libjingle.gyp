#
# libjingle
# Copyright 2012 Google Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#  1. Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright notice,
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.
#  3. The name of the author may not be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
# EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

{
  'includes': ['build/common.gypi'],
  'conditions': [
    ['os_posix == 1 and OS != "mac" and OS != "ios"', {
     'conditions': [
       ['sysroot!=""', {
         'variables': {
           'pkg-config': '../../../build/linux/pkg-config-wrapper "<(sysroot)" "<(target_arch)"',
         },
       }, {
         'variables': {
           'pkg-config': 'pkg-config'
         },
       }],
     ],
    }],
    ['OS=="linux" or OS=="android"', {
      'targets': [
        {
          'target_name': 'libjingle_peerconnection_so',
          'type': 'shared_library',
          'dependencies': [
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:field_trial_default',
            'libjingle_peerconnection',
          ],
          'sources': [
            'app/webrtc/java/jni/classreferenceholder.cc',
            'app/webrtc/java/jni/classreferenceholder.h',
            'app/webrtc/java/jni/jni_helpers.cc',
            'app/webrtc/java/jni/jni_helpers.h',
            'app/webrtc/java/jni/native_handle_impl.cc',
            'app/webrtc/java/jni/native_handle_impl.h',
            'app/webrtc/java/jni/peerconnection_jni.cc',
          ],
          'include_dirs': [
            '<(libyuv_dir)/include',
          ],
          'conditions': [
            ['OS=="linux"', {
              'defines': [
                'HAVE_GTK',
              ],
              'include_dirs': [
                '<(java_home)/include',
                '<(java_home)/include/linux',
              ],
              'conditions': [
                ['use_gtk==1', {
                  'link_settings': {
                    'libraries': [
                      '<!@(pkg-config --libs-only-l gobject-2.0 gthread-2.0'
                          ' gtk+-2.0)',
                    ],
                  },
                }],
              ],
            }],
            ['OS=="android"', {
              'sources': [
                'app/webrtc/java/jni/androidvideocapturer_jni.cc',
                'app/webrtc/java/jni/androidvideocapturer_jni.h',
              ],
              'variables': {
                # This library uses native JNI exports; tell GYP so that the
                # required symbols will be kept.
                'use_native_jni_exports': 1,
              },
            }],
            ['OS=="android" and build_with_chromium==0', {
              'sources': [
                'app/webrtc/java/jni/androidmediacodeccommon.h',
                'app/webrtc/java/jni/androidmediadecoder_jni.cc',
                'app/webrtc/java/jni/androidmediadecoder_jni.h',
                'app/webrtc/java/jni/androidmediaencoder_jni.cc',
                'app/webrtc/java/jni/androidmediaencoder_jni.h',
                'app/webrtc/java/jni/androidnetworkmonitor_jni.cc',
                'app/webrtc/java/jni/androidnetworkmonitor_jni.h',
                'app/webrtc/java/jni/surfacetexturehelper_jni.cc',
                'app/webrtc/java/jni/surfacetexturehelper_jni.h',
              ]
            }],
          ],
        },
        {
          'target_name': 'libjingle_peerconnection_jar',
          'type': 'none',
          'actions': [
            {
              # TODO(jiayl): extract peerconnection_java_files and android_java_files into a webrtc
              # gyp var that can be included here, or better yet, build a proper .jar in webrtc
              # and include it here.
              'variables': {
                'java_src_dir': 'app/webrtc/java/src',
                'webrtc_base_dir': '<(webrtc_root)/base',
                'webrtc_modules_dir': '<(webrtc_root)/modules',
                'build_jar_log': '<(INTERMEDIATE_DIR)/build_jar.log',
                'peerconnection_java_files': [
                  'app/webrtc/java/src/org/webrtc/AudioSource.java',
                  'app/webrtc/java/src/org/webrtc/AudioTrack.java',
                  'app/webrtc/java/src/org/webrtc/CallSessionFileRotatingLogSink.java',
                  'app/webrtc/java/src/org/webrtc/DataChannel.java',
                  'app/webrtc/java/src/org/webrtc/IceCandidate.java',
                  'app/webrtc/java/src/org/webrtc/MediaConstraints.java',
                  'app/webrtc/java/src/org/webrtc/MediaSource.java',
                  'app/webrtc/java/src/org/webrtc/MediaStream.java',
                  'app/webrtc/java/src/org/webrtc/MediaStreamTrack.java',
                  'app/webrtc/java/src/org/webrtc/PeerConnectionFactory.java',
                  'app/webrtc/java/src/org/webrtc/PeerConnection.java',
                  'app/webrtc/java/src/org/webrtc/RtpReceiver.java',
                  'app/webrtc/java/src/org/webrtc/RtpSender.java',
                  'app/webrtc/java/src/org/webrtc/SdpObserver.java',
                  'app/webrtc/java/src/org/webrtc/StatsObserver.java',
                  'app/webrtc/java/src/org/webrtc/StatsReport.java',
                  'app/webrtc/java/src/org/webrtc/SessionDescription.java',
                  'app/webrtc/java/src/org/webrtc/VideoCapturer.java',
                  'app/webrtc/java/src/org/webrtc/VideoRenderer.java',
                  'app/webrtc/java/src/org/webrtc/VideoSource.java',
                  'app/webrtc/java/src/org/webrtc/VideoTrack.java',
                  '<(webrtc_base_dir)/java/src/org/webrtc/Logging.java',
                ],
                'android_java_files': [
                  'app/webrtc/java/android/org/webrtc/Camera2Enumerator.java',
                  'app/webrtc/java/android/org/webrtc/CameraEnumerationAndroid.java',
                  'app/webrtc/java/android/org/webrtc/CameraEnumerator.java',
                  'app/webrtc/java/android/org/webrtc/EglBase.java',
                  'app/webrtc/java/android/org/webrtc/GlRectDrawer.java',
                  'app/webrtc/java/android/org/webrtc/GlShader.java',
                  'app/webrtc/java/android/org/webrtc/GlUtil.java',
                  'app/webrtc/java/android/org/webrtc/GlTextureFrameBuffer.java',
                  'app/webrtc/java/android/org/webrtc/NetworkMonitor.java',
                  'app/webrtc/java/android/org/webrtc/NetworkMonitorAutoDetect.java',
                  'app/webrtc/java/android/org/webrtc/RendererCommon.java',
                  'app/webrtc/java/android/org/webrtc/SurfaceTextureHelper.java',
                  'app/webrtc/java/android/org/webrtc/SurfaceViewRenderer.java',
                  'app/webrtc/java/android/org/webrtc/ThreadUtils.java',
                  'app/webrtc/java/android/org/webrtc/VideoCapturerAndroid.java',
                  'app/webrtc/java/android/org/webrtc/VideoRendererGui.java',
                  'app/webrtc/java/src/org/webrtc/MediaCodecVideoDecoder.java',
                  'app/webrtc/java/src/org/webrtc/MediaCodecVideoEncoder.java',
                  '<(webrtc_modules_dir)/video_render/android/java/src/org/webrtc/videoengine/ViEAndroidGLES20.java',
                  '<(webrtc_modules_dir)/video_render/android/java/src/org/webrtc/videoengine/ViERenderer.java',
                  '<(webrtc_modules_dir)/video_render/android/java/src/org/webrtc/videoengine/ViESurfaceRenderer.java',
                  '<(webrtc_modules_dir)/audio_device/android/java/src/org/webrtc/voiceengine/BuildInfo.java',
                  '<(webrtc_modules_dir)/audio_device/android/java/src/org/webrtc/voiceengine/WebRtcAudioEffects.java',
                  '<(webrtc_modules_dir)/audio_device/android/java/src/org/webrtc/voiceengine/WebRtcAudioManager.java',
                  '<(webrtc_modules_dir)/audio_device/android/java/src/org/webrtc/voiceengine/WebRtcAudioUtils.java',
                  '<(webrtc_modules_dir)/audio_device/android/java/src/org/webrtc/voiceengine/WebRtcAudioRecord.java',
                  '<(webrtc_modules_dir)/audio_device/android/java/src/org/webrtc/voiceengine/WebRtcAudioTrack.java',
                ],
              },
              'action_name': 'create_jar',
              'inputs': [
                'build/build_jar.sh',
                '<@(java_files)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/libjingle_peerconnection.jar',
              ],
              'conditions': [
                ['OS=="android"', {
                  'variables': {
                    'java_files': ['<@(peerconnection_java_files)', '<@(android_java_files)'],
                    'build_classpath': '<(java_src_dir):<(DEPTH)/third_party/android_tools/sdk/platforms/android-<(android_sdk_version)/android.jar',
                  },
                }, {
                  'variables': {
                    'java_files': ['<@(peerconnection_java_files)'],
                    'build_classpath': '<(java_src_dir)',
                  },
                }],
              ],
              'action': [
                'bash', '-ec',
                'mkdir -p <(INTERMEDIATE_DIR) && '
                '{ build/build_jar.sh <(java_home) <@(_outputs) '
                '      <(INTERMEDIATE_DIR)/build_jar.tmp '
                '      <(build_classpath) <@(java_files) '
                '      > <(build_jar_log) 2>&1 || '
                '  { cat <(build_jar_log) ; exit 1; } }'
              ],
            },
          ],
          'dependencies': [
            'libjingle_peerconnection_so',
          ],
        },
      ],
    }],
    ['OS=="android"', {
      'targets': [
        {
          # |libjingle_peerconnection_java| builds a jar file with name
          # libjingle_peerconnection_java.jar using Chromes build system.
          # It includes all Java files needed to setup a PeeerConnection call
          # from Android.
          # TODO(perkj): Consider replacing the use of
          # libjingle_peerconnection_jar with this target everywhere.
          'target_name': 'libjingle_peerconnection_java',
          'type': 'none',
          'dependencies': [
            'libjingle_peerconnection_so',
          ],
          'variables': {
            'java_in_dir': 'app/webrtc/java',
            'webrtc_base_dir': '<(webrtc_root)/base',
            'webrtc_modules_dir': '<(webrtc_root)/modules',
            'additional_src_dirs' : [
              'app/webrtc/java/android',
              '<(webrtc_base_dir)/java/src',
              '<(webrtc_modules_dir)/audio_device/android/java/src',
              '<(webrtc_modules_dir)/video_render/android/java/src',
            ],
          },
          'includes': ['../build/java.gypi'],
        }, # libjingle_peerconnection_java
      ]
    }],
    ['OS=="ios" or (OS=="mac" and target_arch!="ia32" and mac_sdk>="10.7")', {
      # The >= 10.7 above is required for ARC.
      'targets': [
        {
          'target_name': 'libjingle_peerconnection_objc',
          'type': 'static_library',
          'dependencies': [
            'libjingle_peerconnection',
          ],
          'sources': [
            'app/webrtc/objc/RTCAudioTrack+Internal.h',
            'app/webrtc/objc/RTCAudioTrack.mm',
            'app/webrtc/objc/RTCDataChannel+Internal.h',
            'app/webrtc/objc/RTCDataChannel.mm',
            'app/webrtc/objc/RTCEnumConverter.h',
            'app/webrtc/objc/RTCEnumConverter.mm',
            'app/webrtc/objc/RTCFileLogger.mm',
            'app/webrtc/objc/RTCI420Frame+Internal.h',
            'app/webrtc/objc/RTCI420Frame.mm',
            'app/webrtc/objc/RTCICECandidate+Internal.h',
            'app/webrtc/objc/RTCICECandidate.mm',
            'app/webrtc/objc/RTCICEServer+Internal.h',
            'app/webrtc/objc/RTCICEServer.mm',
            'app/webrtc/objc/RTCLogging.mm',
            'app/webrtc/objc/RTCMediaConstraints+Internal.h',
            'app/webrtc/objc/RTCMediaConstraints.mm',
            'app/webrtc/objc/RTCMediaConstraintsNative.cc',
            'app/webrtc/objc/RTCMediaConstraintsNative.h',
            'app/webrtc/objc/RTCMediaSource+Internal.h',
            'app/webrtc/objc/RTCMediaSource.mm',
            'app/webrtc/objc/RTCMediaStream+Internal.h',
            'app/webrtc/objc/RTCMediaStream.mm',
            'app/webrtc/objc/RTCMediaStreamTrack+Internal.h',
            'app/webrtc/objc/RTCMediaStreamTrack.mm',
            'app/webrtc/objc/RTCOpenGLVideoRenderer.mm',
            'app/webrtc/objc/RTCPair.m',
            'app/webrtc/objc/RTCPeerConnection+Internal.h',
            'app/webrtc/objc/RTCPeerConnection.mm',
            'app/webrtc/objc/RTCPeerConnectionFactory.mm',
            'app/webrtc/objc/RTCPeerConnectionInterface+Internal.h',
            'app/webrtc/objc/RTCPeerConnectionInterface.mm',
            'app/webrtc/objc/RTCPeerConnectionObserver.h',
            'app/webrtc/objc/RTCPeerConnectionObserver.mm',
            'app/webrtc/objc/RTCSessionDescription+Internal.h',
            'app/webrtc/objc/RTCSessionDescription.mm',
            'app/webrtc/objc/RTCStatsReport+Internal.h',
            'app/webrtc/objc/RTCStatsReport.mm',
            'app/webrtc/objc/RTCVideoCapturer+Internal.h',
            'app/webrtc/objc/RTCVideoCapturer.mm',
            'app/webrtc/objc/RTCVideoRendererAdapter.h',
            'app/webrtc/objc/RTCVideoRendererAdapter.mm',
            'app/webrtc/objc/RTCVideoSource+Internal.h',
            'app/webrtc/objc/RTCVideoSource.mm',
            'app/webrtc/objc/RTCVideoTrack+Internal.h',
            'app/webrtc/objc/RTCVideoTrack.mm',
            'app/webrtc/objc/public/RTCAudioSource.h',
            'app/webrtc/objc/public/RTCAudioTrack.h',
            'app/webrtc/objc/public/RTCDataChannel.h',
            'app/webrtc/objc/public/RTCFileLogger.h',
            'app/webrtc/objc/public/RTCI420Frame.h',
            'app/webrtc/objc/public/RTCICECandidate.h',
            'app/webrtc/objc/public/RTCICEServer.h',
            'app/webrtc/objc/public/RTCLogging.h',
            'app/webrtc/objc/public/RTCMediaConstraints.h',
            'app/webrtc/objc/public/RTCMediaSource.h',
            'app/webrtc/objc/public/RTCMediaStream.h',
            'app/webrtc/objc/public/RTCMediaStreamTrack.h',
            'app/webrtc/objc/public/RTCOpenGLVideoRenderer.h',
            'app/webrtc/objc/public/RTCPair.h',
            'app/webrtc/objc/public/RTCPeerConnection.h',
            'app/webrtc/objc/public/RTCPeerConnectionDelegate.h',
            'app/webrtc/objc/public/RTCPeerConnectionFactory.h',
            'app/webrtc/objc/public/RTCPeerConnectionInterface.h',
            'app/webrtc/objc/public/RTCSessionDescription.h',
            'app/webrtc/objc/public/RTCSessionDescriptionDelegate.h',
            'app/webrtc/objc/public/RTCStatsDelegate.h',
            'app/webrtc/objc/public/RTCStatsReport.h',
            'app/webrtc/objc/public/RTCTypes.h',
            'app/webrtc/objc/public/RTCVideoCapturer.h',
            'app/webrtc/objc/public/RTCVideoRenderer.h',
            'app/webrtc/objc/public/RTCVideoSource.h',
            'app/webrtc/objc/public/RTCVideoTrack.h',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(DEPTH)/talk/app/webrtc/objc/public',
            ],
          },
          'include_dirs': [
            '<(DEPTH)/talk/app/webrtc',
            '<(DEPTH)/talk/app/webrtc/objc',
            '<(DEPTH)/talk/app/webrtc/objc/public',
          ],
          'link_settings': {
            'libraries': [
              '-lstdc++',
            ],
          },
          'all_dependent_settings': {
            'xcode_settings': {
              'CLANG_ENABLE_OBJC_ARC': 'YES',
            },
          },
          'xcode_settings': {
            'CLANG_ENABLE_OBJC_ARC': 'YES',
            # common.gypi enables this for mac but we want this to be disabled
            # like it is for ios.
            'CLANG_WARN_OBJC_MISSING_PROPERTY_SYNTHESIS': 'NO',
          },
          'conditions': [
            ['OS=="ios"', {
              'sources': [
                'app/webrtc/objc/avfoundationvideocapturer.h',
                'app/webrtc/objc/avfoundationvideocapturer.mm',
                'app/webrtc/objc/RTCAVFoundationVideoSource+Internal.h',
                'app/webrtc/objc/RTCAVFoundationVideoSource.mm',
                'app/webrtc/objc/RTCEAGLVideoView.m',
                'app/webrtc/objc/public/RTCEAGLVideoView.h',
                'app/webrtc/objc/public/RTCAVFoundationVideoSource.h',
              ],
              'link_settings': {
                'xcode_settings': {
                  'OTHER_LDFLAGS': [
                    '-framework CoreGraphics',
                    '-framework GLKit',
                  ],
                },
              },
            }],
            ['OS=="mac"', {
              'sources': [
                'app/webrtc/objc/RTCNSGLVideoView.m',
                'app/webrtc/objc/public/RTCNSGLVideoView.h',
              ],
              'xcode_settings': {
                # Need to build against 10.7 framework for full ARC support
                # on OSX.
                'MACOSX_DEPLOYMENT_TARGET' : '10.7',
                # RTCVideoTrack.mm uses code with partial availability.
                # https://code.google.com/p/webrtc/issues/detail?id=4695
                'WARNING_CFLAGS!': ['-Wpartial-availability'],
              },
              'link_settings': {
                'xcode_settings': {
                  'OTHER_LDFLAGS': [
                    '-framework Cocoa',
                  ],
                },
              },
            }],
          ],
        },  # target libjingle_peerconnection_objc
      ],
    }],
  ],

  'targets': [
    {
      'target_name': 'libjingle',
      'type': 'none',
      'dependencies': [
        '<(webrtc_root)/base/base.gyp:rtc_base',
      ],
      'conditions': [
        ['build_json==1', {
          'dependencies': [
            '<(DEPTH)/third_party/jsoncpp/jsoncpp.gyp:jsoncpp',
          ],
          'export_dependent_settings': [
            '<(DEPTH)/third_party/jsoncpp/jsoncpp.gyp:jsoncpp',
          ],
        }],
        ['build_expat==1', {
          'dependencies': [
            '<(DEPTH)/third_party/expat/expat.gyp:expat',
          ],
          'export_dependent_settings': [
            '<(DEPTH)/third_party/expat/expat.gyp:expat',
          ],
        }],
      ],
    },  # target libjingle
    {
      'target_name': 'libjingle_media',
      'type': 'static_library',
      'dependencies': [
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
        'libjingle',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(libyuv_dir)/include',
        ],
      },
      'sources': [
        'media/base/audioframe.h',
        'media/base/audiorenderer.h',
        'media/base/capturemanager.cc',
        'media/base/capturemanager.h',
        'media/base/capturerenderadapter.cc',
        'media/base/capturerenderadapter.h',
        'media/base/codec.cc',
        'media/base/codec.h',
        'media/base/constants.cc',
        'media/base/constants.h',
        'media/base/cpuid.cc',
        'media/base/cpuid.h',
        'media/base/cryptoparams.h',
        'media/base/device.h',
        'media/base/fakescreencapturerfactory.h',
        'media/base/hybriddataengine.h',
        'media/base/mediachannel.h',
        'media/base/mediacommon.h',
        'media/base/mediaengine.cc',
        'media/base/mediaengine.h',
        'media/base/rtpdataengine.cc',
        'media/base/rtpdataengine.h',
        'media/base/rtpdump.cc',
        'media/base/rtpdump.h',
        'media/base/rtputils.cc',
        'media/base/rtputils.h',
        'media/base/screencastid.h',
        'media/base/streamparams.cc',
        'media/base/streamparams.h',
        'media/base/videoadapter.cc',
        'media/base/videoadapter.h',
        'media/base/videocapturer.cc',
        'media/base/videocapturer.h',
        'media/base/videocapturerfactory.h',
        'media/base/videocommon.cc',
        'media/base/videocommon.h',
        'media/base/videoframe.cc',
        'media/base/videoframe.h',
        'media/base/videoframefactory.cc',
        'media/base/videoframefactory.h',
        'media/base/videorenderer.h',
        'media/base/yuvframegenerator.cc',
        'media/base/yuvframegenerator.h',
        'media/devices/deviceinfo.h',
        'media/devices/devicemanager.cc',
        'media/devices/devicemanager.h',
        'media/devices/dummydevicemanager.h',
        'media/devices/filevideocapturer.cc',
        'media/devices/filevideocapturer.h',
        'media/devices/videorendererfactory.h',
        'media/devices/yuvframescapturer.cc',
        'media/devices/yuvframescapturer.h',
        'media/sctp/sctpdataengine.cc',
        'media/sctp/sctpdataengine.h',
        'media/webrtc/simulcast.cc',
        'media/webrtc/simulcast.h',
        'media/webrtc/webrtccommon.h',
        'media/webrtc/webrtcmediaengine.cc',
        'media/webrtc/webrtcmediaengine.h',
        'media/webrtc/webrtcmediaengine.cc',
        'media/webrtc/webrtcvideocapturer.cc',
        'media/webrtc/webrtcvideocapturer.h',
        'media/webrtc/webrtcvideocapturerfactory.h',
        'media/webrtc/webrtcvideocapturerfactory.cc',
        'media/webrtc/webrtcvideodecoderfactory.h',
        'media/webrtc/webrtcvideoencoderfactory.h',
        'media/webrtc/webrtcvideoengine2.cc',
        'media/webrtc/webrtcvideoengine2.h',
        'media/webrtc/webrtcvideoframe.cc',
        'media/webrtc/webrtcvideoframe.h',
        'media/webrtc/webrtcvideoframefactory.cc',
        'media/webrtc/webrtcvideoframefactory.h',
        'media/webrtc/webrtcvoe.h',
        'media/webrtc/webrtcvoiceengine.cc',
        'media/webrtc/webrtcvoiceengine.h',
      ],
      'conditions': [
        ['build_libyuv==1', {
          'dependencies': ['<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv',],
        }],
        ['build_usrsctp==1', {
          'include_dirs': [
            # TODO(jiayl): move this into the direct_dependent_settings of
            # usrsctp.gyp.
            '<(DEPTH)/third_party/usrsctp',
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
          'dependencies': [
            '<(webrtc_root)/modules/modules.gyp:video_capture_module_internal_impl',
            '<(webrtc_root)/modules/modules.gyp:video_render_module_internal_impl',
          ],
        }],
        ['OS=="linux"', {
          'sources': [
            'media/devices/libudevsymboltable.cc',
            'media/devices/libudevsymboltable.h',
            'media/devices/linuxdeviceinfo.cc',
            'media/devices/linuxdevicemanager.cc',
            'media/devices/linuxdevicemanager.h',
            'media/devices/v4llookup.cc',
            'media/devices/v4llookup.h',
          ],
          'conditions': [
            ['use_gtk==1', {
              'sources': [
                'media/devices/gtkvideorenderer.cc',
                'media/devices/gtkvideorenderer.h',
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
            'media/devices/gdivideorenderer.cc',
            'media/devices/gdivideorenderer.h',
            'media/devices/win32deviceinfo.cc',
            'media/devices/win32devicemanager.cc',
            'media/devices/win32devicemanager.h',
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
            'media/devices/macdeviceinfo.cc',
            'media/devices/macdevicemanager.cc',
            'media/devices/macdevicemanager.h',
            'media/devices/macdevicemanagermm.mm',
          ],
          'conditions': [
            ['target_arch=="ia32"', {
              'sources': [
                'media/devices/carbonvideorenderer.cc',
                'media/devices/carbonvideorenderer.h',
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
            'media/devices/mobiledevicemanager.cc',
          ],
          'include_dirs': [
            # TODO(sjlee) Remove when vp8 is building for iOS.  vp8 pulls in
            # libjpeg which pulls in libyuv which currently disabled.
            '../third_party/libyuv/include',
          ],
        }],
        ['OS=="android"', {
          'sources': [
            'media/devices/mobiledevicemanager.cc',
          ],
        }],
      ],
    },  # target libjingle_media
    {
      'target_name': 'libjingle_p2p',
      'type': 'static_library',
      'dependencies': [
        'libjingle',
        'libjingle_media',
      ],
      'conditions': [
        ['build_libsrtp==1', {
          'dependencies': [
            '<(DEPTH)/third_party/libsrtp/libsrtp.gyp:libsrtp',
          ],
        }],
      ],
      'include_dirs': [
        '<(DEPTH)/testing/gtest/include',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/testing/gtest/include',
        ],
      },
      'sources': [
        'session/media/audiomonitor.cc',
        'session/media/audiomonitor.h',
        'session/media/bundlefilter.cc',
        'session/media/bundlefilter.h',
        'session/media/channel.cc',
        'session/media/channel.h',
        'session/media/channelmanager.cc',
        'session/media/channelmanager.h',
        'session/media/currentspeakermonitor.cc',
        'session/media/currentspeakermonitor.h',
        'session/media/mediamonitor.cc',
        'session/media/mediamonitor.h',
        'session/media/mediasession.cc',
        'session/media/mediasession.h',
        'session/media/mediasink.h',
        'session/media/rtcpmuxfilter.cc',
        'session/media/rtcpmuxfilter.h',
        'session/media/srtpfilter.cc',
        'session/media/srtpfilter.h',
        'session/media/voicechannel.h',
      ],
    },  # target libjingle_p2p
    {
      'target_name': 'libjingle_peerconnection',
      'type': 'static_library',
      'dependencies': [
        'libjingle',
        'libjingle_media',
        'libjingle_p2p',
      ],
      'sources': [
        'app/webrtc/audiotrack.cc',
        'app/webrtc/audiotrack.h',
        'app/webrtc/datachannel.cc',
        'app/webrtc/datachannel.h',
        'app/webrtc/datachannelinterface.h',
        'app/webrtc/dtlsidentitystore.cc',
        'app/webrtc/dtlsidentitystore.h',
        'app/webrtc/dtmfsender.cc',
        'app/webrtc/dtmfsender.h',
        'app/webrtc/dtmfsenderinterface.h',
        'app/webrtc/fakeportallocatorfactory.h',
        'app/webrtc/jsep.h',
        'app/webrtc/jsepicecandidate.cc',
        'app/webrtc/jsepicecandidate.h',
        'app/webrtc/jsepsessiondescription.cc',
        'app/webrtc/jsepsessiondescription.h',
        'app/webrtc/localaudiosource.cc',
        'app/webrtc/localaudiosource.h',
        'app/webrtc/mediaconstraintsinterface.cc',
        'app/webrtc/mediaconstraintsinterface.h',
        'app/webrtc/mediacontroller.cc',
        'app/webrtc/mediacontroller.h',
        'app/webrtc/mediastream.cc',
        'app/webrtc/mediastream.h',
        'app/webrtc/mediastreaminterface.h',
        'app/webrtc/mediastreamprovider.h',
        'app/webrtc/mediastreamproxy.h',
        'app/webrtc/mediastreamsignaling.cc',
        'app/webrtc/mediastreamsignaling.h',
        'app/webrtc/mediastreamtrack.h',
        'app/webrtc/mediastreamtrackproxy.h',
        'app/webrtc/notifier.h',
        'app/webrtc/peerconnection.cc',
        'app/webrtc/peerconnection.h',
        'app/webrtc/peerconnectionfactory.cc',
        'app/webrtc/peerconnectionfactory.h',
        'app/webrtc/peerconnectionfactoryproxy.h',
        'app/webrtc/peerconnectioninterface.h',
        'app/webrtc/peerconnectionproxy.h',
        'app/webrtc/portallocatorfactory.cc',
        'app/webrtc/portallocatorfactory.h',
        'app/webrtc/proxy.h',
        'app/webrtc/remoteaudiosource.cc',
        'app/webrtc/remoteaudiosource.h',
        'app/webrtc/remotevideocapturer.cc',
        'app/webrtc/remotevideocapturer.h',
        'app/webrtc/rtpreceiver.cc',
        'app/webrtc/rtpreceiver.h',
        'app/webrtc/rtpreceiverinterface.h',
        'app/webrtc/rtpsender.cc',
        'app/webrtc/rtpsender.h',
        'app/webrtc/rtpsenderinterface.h',
        'app/webrtc/sctputils.cc',
        'app/webrtc/sctputils.h',
        'app/webrtc/statscollector.cc',
        'app/webrtc/statscollector.h',
        'app/webrtc/statstypes.cc',
        'app/webrtc/statstypes.h',
        'app/webrtc/streamcollection.h',
        'app/webrtc/videosource.cc',
        'app/webrtc/videosource.h',
        'app/webrtc/videosourceinterface.h',
        'app/webrtc/videosourceproxy.h',
        'app/webrtc/videotrack.cc',
        'app/webrtc/videotrack.h',
        'app/webrtc/videotrackrenderers.cc',
        'app/webrtc/videotrackrenderers.h',
        'app/webrtc/webrtcsdp.cc',
        'app/webrtc/webrtcsdp.h',
        'app/webrtc/webrtcsession.cc',
        'app/webrtc/webrtcsession.h',
        'app/webrtc/webrtcsessiondescriptionfactory.cc',
        'app/webrtc/webrtcsessiondescriptionfactory.h',
      ],
      'conditions': [
        ['OS=="android" and build_with_chromium==0', {
          'sources': [
            'app/webrtc/androidvideocapturer.h',
            'app/webrtc/androidvideocapturer.cc',
           ],
        }],
      ],
    },  # target libjingle_peerconnection
  ],
}
