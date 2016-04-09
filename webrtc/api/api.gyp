# Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [ '../build/common.gypi', ],
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
            'androidvideocapturer.cc',
            'androidvideocapturer.h',
            'java/jni/androidmediacodeccommon.h',
            'java/jni/androidmediadecoder_jni.cc',
            'java/jni/androidmediadecoder_jni.h',
            'java/jni/androidmediaencoder_jni.cc',
            'java/jni/androidmediaencoder_jni.h',
            'java/jni/androidnetworkmonitor_jni.cc',
            'java/jni/androidnetworkmonitor_jni.h',
            'java/jni/androidvideocapturer_jni.cc',
            'java/jni/androidvideocapturer_jni.h',
            'java/jni/surfacetexturehelper_jni.cc',
            'java/jni/surfacetexturehelper_jni.h',
            'java/jni/classreferenceholder.cc',
            'java/jni/classreferenceholder.h',
            'java/jni/jni_helpers.cc',
            'java/jni/jni_helpers.h',
            'java/jni/native_handle_impl.cc',
            'java/jni/native_handle_impl.h',
            'java/jni/peerconnection_jni.cc',
          ],
          'include_dirs': [
            '<(libyuv_dir)/include',
          ],
          # TODO(kjellander): Make the code compile without disabling these flags.
          # See https://bugs.chromium.org/p/webrtc/issues/detail?id=3307
          'cflags': [
            '-Wno-sign-compare',
            '-Wno-unused-variable',
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
        },
        {
          'target_name': 'libjingle_peerconnection_so',
          'type': 'shared_library',
          'dependencies': [
            'libjingle_peerconnection',
            'libjingle_peerconnection_jni',
          ],
          'sources': [
           'java/jni/jni_onload.cc',
          ],
          'variables': {
            # This library uses native JNI exports; tell GYP so that the
            # required symbols will be kept.
            'use_native_jni_exports': 1,
          },
        },
        {
          # |libjingle_peerconnection_java| builds a jar file with name
          # libjingle_peerconnection_java.jar using Chrome's build system.
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
            'java_in_dir': 'java',
            'webrtc_base_dir': '<(webrtc_root)/base',
            'webrtc_modules_dir': '<(webrtc_root)/modules',
            'additional_src_dirs' : [
              'java/android',
              '<(webrtc_base_dir)/java/src',
              '<(webrtc_modules_dir)/audio_device/android/java/src',
              '<(webrtc_modules_dir)/video_render/android/java/src',
            ],
          },
          'includes': ['../../build/java.gypi'],
        }, # libjingle_peerconnection_java
      ]
    }],
    ['OS=="ios" or (OS=="mac" and mac_deployment_target=="10.7")', {
      'targets': [
        {
          'target_name': 'rtc_api_objc',
          'type': 'static_library',
          'includes': [
            '../build/objc_common.gypi',
          ],
          'dependencies': [
            '<(webrtc_root)/base/base.gyp:rtc_base_objc',
            'libjingle_peerconnection',
          ],
          'sources': [
            'objc/RTCAudioTrack+Private.h',
            'objc/RTCAudioTrack.h',
            'objc/RTCAudioTrack.mm',
            'objc/RTCConfiguration+Private.h',
            'objc/RTCConfiguration.h',
            'objc/RTCConfiguration.mm',
            'objc/RTCDataChannel+Private.h',
            'objc/RTCDataChannel.h',
            'objc/RTCDataChannel.mm',
            'objc/RTCDataChannelConfiguration+Private.h',
            'objc/RTCDataChannelConfiguration.h',
            'objc/RTCDataChannelConfiguration.mm',
            'objc/RTCIceCandidate+Private.h',
            'objc/RTCIceCandidate.h',
            'objc/RTCIceCandidate.mm',
            'objc/RTCIceServer+Private.h',
            'objc/RTCIceServer.h',
            'objc/RTCIceServer.mm',
            'objc/RTCMediaConstraints+Private.h',
            'objc/RTCMediaConstraints.h',
            'objc/RTCMediaConstraints.mm',
            'objc/RTCMediaStream+Private.h',
            'objc/RTCMediaStream.h',
            'objc/RTCMediaStream.mm',
            'objc/RTCMediaStreamTrack+Private.h',
            'objc/RTCMediaStreamTrack.h',
            'objc/RTCMediaStreamTrack.mm',
            'objc/RTCOpenGLVideoRenderer.h',
            'objc/RTCOpenGLVideoRenderer.mm',
            'objc/RTCPeerConnection+DataChannel.mm',
            'objc/RTCPeerConnection+Private.h',
            'objc/RTCPeerConnection+Stats.mm',
            'objc/RTCPeerConnection.h',
            'objc/RTCPeerConnection.mm',
            'objc/RTCPeerConnectionFactory+Private.h',
            'objc/RTCPeerConnectionFactory.h',
            'objc/RTCPeerConnectionFactory.mm',
            'objc/RTCRtpEncodingParameters+Private.h',
            'objc/RTCRtpEncodingParameters.h',
            'objc/RTCRtpEncodingParameters.mm',
            'objc/RTCRtpParameters+Private.h',
            'objc/RTCRtpParameters.h',
            'objc/RTCRtpParameters.mm',
            'objc/RTCRtpSender+Private.h',
            'objc/RTCRtpSender.h',
            'objc/RTCRtpSender.mm',
            'objc/RTCSessionDescription+Private.h',
            'objc/RTCSessionDescription.h',
            'objc/RTCSessionDescription.mm',
            'objc/RTCStatsReport+Private.h',
            'objc/RTCStatsReport.h',
            'objc/RTCStatsReport.mm',
            'objc/RTCVideoFrame+Private.h',
            'objc/RTCVideoFrame.h',
            'objc/RTCVideoFrame.mm',
            'objc/RTCVideoRenderer.h',
            'objc/RTCVideoRendererAdapter+Private.h',
            'objc/RTCVideoRendererAdapter.h',
            'objc/RTCVideoRendererAdapter.mm',
            'objc/RTCVideoSource+Private.h',
            'objc/RTCVideoSource.h',
            'objc/RTCVideoSource.mm',
            'objc/RTCVideoTrack+Private.h',
            'objc/RTCVideoTrack.h',
            'objc/RTCVideoTrack.mm',
          ],
          # TODO(hjon): Make this compile without linking to libstdc++
          # See https://bugs.chromium.org/p/webrtc/issues/detail?id=5593
          'link_settings': {
            'libraries': [
              '-lstdc++',
            ],
          },
          'conditions': [
            ['OS=="ios"', {
              'sources': [
                'objc/RTCAVFoundationVideoSource+Private.h',
                'objc/RTCAVFoundationVideoSource.h',
                'objc/RTCAVFoundationVideoSource.mm',
                'objc/RTCEAGLVideoView.h',
                'objc/RTCEAGLVideoView.m',
                'objc/avfoundationvideocapturer.h',
                'objc/avfoundationvideocapturer.mm',
              ],
              'all_dependent_settings': {
                'xcode_settings': {
                  'OTHER_LDFLAGS': [
                    '-framework CoreGraphics',
                    '-framework GLKit',
                    '-framework OpenGLES',
                    '-framework QuartzCore',
                  ]
                }
              },
              # TODO(kjellander): Make the code compile without disabling these.
              # See https://bugs.chromium.org/p/webrtc/issues/detail?id=3307
              'cflags': [
                '-Wno-return-type',
              ],
              'xcode_settings': {
                'WARNING_CFLAGS': [
                  '-Wno-return-type',
                ],
              },
            }],
            ['OS=="mac"', {
              'sources': [
                'objc/RTCNSGLVideoView.h',
                'objc/RTCNSGLVideoView.m',
              ],
              'link_settings': {
                'xcode_settings': {
                  'OTHER_LDFLAGS': [
                    '-framework OpenGL',
                  ],
                },
              },
            }],
          ],
        }
      ],
    }],  # OS=="ios"
  ],  # conditions
  'targets': [
    {
      'target_name': 'libjingle_peerconnection',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/media/media.gyp:rtc_media',
        '<(webrtc_root)/pc/pc.gyp:rtc_pc',
      ],
      'sources': [
        'audiotrack.cc',
        'audiotrack.h',
        'datachannel.cc',
        'datachannel.h',
        'datachannelinterface.h',
        'dtlsidentitystore.cc',
        'dtlsidentitystore.h',
        'dtmfsender.cc',
        'dtmfsender.h',
        'dtmfsenderinterface.h',
        'jsep.h',
        'jsepicecandidate.cc',
        'jsepicecandidate.h',
        'jsepsessiondescription.cc',
        'jsepsessiondescription.h',
        'localaudiosource.cc',
        'localaudiosource.h',
        'mediaconstraintsinterface.cc',
        'mediaconstraintsinterface.h',
        'mediacontroller.cc',
        'mediacontroller.h',
        'mediastream.cc',
        'mediastream.h',
        'mediastreaminterface.h',
        'mediastreamobserver.cc',
        'mediastreamobserver.h',
        'mediastreamprovider.h',
        'mediastreamproxy.h',
        'mediastreamtrack.h',
        'mediastreamtrackproxy.h',
        'notifier.h',
        'peerconnection.cc',
        'peerconnection.h',
        'peerconnectionfactory.cc',
        'peerconnectionfactory.h',
        'peerconnectionfactoryproxy.h',
        'peerconnectioninterface.h',
        'peerconnectionproxy.h',
        'proxy.h',
        'remoteaudiosource.cc',
        'remoteaudiosource.h',
        'rtpparameters.h',
        'rtpreceiver.cc',
        'rtpreceiver.h',
        'rtpreceiverinterface.h',
        'rtpsender.cc',
        'rtpsender.h',
        'rtpsenderinterface.h',
        'sctputils.cc',
        'sctputils.h',
        'statscollector.cc',
        'statscollector.h',
        'statstypes.cc',
        'statstypes.h',
        'streamcollection.h',
        'videocapturertracksource.cc',
        'videocapturertracksource.h',
        'videosourceproxy.h',
        'videotrack.cc',
        'videotrack.h',
        'videotracksource.cc',
        'videotracksource.h',
        'webrtcsdp.cc',
        'webrtcsdp.h',
        'webrtcsession.cc',
        'webrtcsession.h',
        'webrtcsessiondescriptionfactory.cc',
        'webrtcsessiondescriptionfactory.h',
      ],
      # TODO(kjellander): Make the code compile without disabling these flags.
      # See https://bugs.chromium.org/p/webrtc/issues/detail?id=3307
      'cflags': [
        '-Wno-sign-compare',
      ],
      'cflags_cc!': [
        '-Wnon-virtual-dtor',
        '-Woverloaded-virtual',
      ],
      'conditions': [
        ['clang==1', {
          'cflags!': [
            '-Wextra',
          ],
          'xcode_settings': {
            'WARNING_CFLAGS!': ['-Wextra'],
          },
        }, {
          'cflags': [
            '-Wno-maybe-uninitialized',  # Only exists for GCC.
          ],
        }],
        ['OS=="win"', {
          # Disable warning for signed/unsigned mismatch.
          'msvs_settings': {
            'VCCLCompilerTool': {
              'AdditionalOptions!': ['/we4389'],
            },
          },
        }],
        ['OS=="win" and clang==1', {
          'msvs_settings': {
            'VCCLCompilerTool': {
              'AdditionalOptions': [
                '-Wno-sign-compare',
              ],
            },
          },
        }],
      ],
    },  # target libjingle_peerconnection
  ],  # targets
}
