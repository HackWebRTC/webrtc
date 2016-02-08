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
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'libjingle_peerconnection_jni',
          'type': 'static_library',
          'dependencies': [
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:field_trial_default',
            'libjingle_peerconnection',
          ],
          'sources': [
            'app/webrtc/androidvideocapturer.cc',
            'app/webrtc/androidvideocapturer.h',
            'app/webrtc/java/jni/androidmediacodeccommon.h',
            'app/webrtc/java/jni/androidmediadecoder_jni.cc',
            'app/webrtc/java/jni/androidmediadecoder_jni.h',
            'app/webrtc/java/jni/androidmediaencoder_jni.cc',
            'app/webrtc/java/jni/androidmediaencoder_jni.h',
            'app/webrtc/java/jni/androidnetworkmonitor_jni.cc',
            'app/webrtc/java/jni/androidnetworkmonitor_jni.h',
            'app/webrtc/java/jni/androidvideocapturer_jni.cc',
            'app/webrtc/java/jni/androidvideocapturer_jni.h',
            'app/webrtc/java/jni/classreferenceholder.cc',
            'app/webrtc/java/jni/classreferenceholder.h',
            'app/webrtc/java/jni/eglbase_jni.cc',
            'app/webrtc/java/jni/eglbase_jni.h',
            'app/webrtc/java/jni/jni_helpers.cc',
            'app/webrtc/java/jni/jni_helpers.h',
            'app/webrtc/java/jni/native_handle_impl.cc',
            'app/webrtc/java/jni/native_handle_impl.h',
            'app/webrtc/java/jni/peerconnection_jni.cc',
            'app/webrtc/java/jni/surfacetexturehelper_jni.cc',
            'app/webrtc/java/jni/surfacetexturehelper_jni.h',
          ],
          'include_dirs': [
            '<(libyuv_dir)/include',
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
          ],
        },
        {
          'target_name': 'libjingle_peerconnection_so',
          'type': 'shared_library',
          'dependencies': [
            'libjingle_peerconnection',
            'libjingle_peerconnection_jni',
          ],
          'sources': [
           'app/webrtc/java/jni/jni_onload.cc',
          ],
          'variables': {
            # This library uses native JNI exports; tell GYP so that the
            # required symbols will be kept.
            'use_native_jni_exports': 1,
          },
        },
        {
          # |libjingle_peerconnection_java| builds a jar file with name
          # libjingle_peerconnection_java.jar using Chromes build system.
          # It includes all Java files needed to setup a PeeerConnection call
          # from Android.
          'target_name': 'libjingle_peerconnection_java',
          'type': 'none',
          'dependencies': [
            'libjingle_peerconnection_so',
          ],
          'variables': {
            # Designate as Chromium code and point to our lint settings to
            # enable linting of the WebRTC code (this is the only way to make
            # lint_action invoke the Android linter).
            'android_manifest_path': '<(webrtc_root)/build/android/AndroidManifest.xml',
            'suppressions_file': '<(webrtc_root)/build/android/suppressions.xml',
            'chromium_code': 1,
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
    ['OS=="ios" or (OS=="mac" and target_arch!="ia32")', {
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
            # Disabled due to failing when compiled with -Wall, see
            # https://bugs.chromium.org/p/webrtc/issues/detail?id=5397
            'WARNING_CFLAGS': ['-Wno-unused-property-ivar'],
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
              'dependencies': [
                '<(webrtc_root)/base/base.gyp:rtc_base_objc',
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
      'target_name': 'libjingle_p2p',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/base/base.gyp:rtc_base',
        '<(webrtc_root)/media/media.gyp:rtc_media',
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
      'include_dirs!': [
        '<(DEPTH)/webrtc',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/testing/gtest/include',
        ],
        'include_dirs!': [
          '<(DEPTH)/webrtc',
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
        '<(webrtc_root)/base/base.gyp:rtc_base',
        '<(webrtc_root)/media/media.gyp:rtc_media',
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
        'app/webrtc/mediastreamobserver.cc',
        'app/webrtc/mediastreamobserver.h',
        'app/webrtc/mediastreamprovider.h',
        'app/webrtc/mediastreamproxy.h',
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
    },  # target libjingle_peerconnection
  ],
}
