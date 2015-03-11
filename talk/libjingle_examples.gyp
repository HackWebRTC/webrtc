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
  'includes': [
    'build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'relayserver',
      'type': 'executable',
      'dependencies': [
        'libjingle.gyp:libjingle',
        'libjingle.gyp:libjingle_p2p',
      ],
      'sources': [
        'examples/relayserver/relayserver_main.cc',
      ],
    },  # target relayserver
    {
      'target_name': 'stunserver',
      'type': 'executable',
      'dependencies': [
        'libjingle.gyp:libjingle',
        'libjingle.gyp:libjingle_p2p',
      ],
      'sources': [
        'examples/stunserver/stunserver_main.cc',
      ],
    },  # target stunserver
    {
      'target_name': 'turnserver',
      'type': 'executable',
      'dependencies': [
        'libjingle.gyp:libjingle',
        'libjingle.gyp:libjingle_p2p',
      ],
      'sources': [
        'examples/turnserver/turnserver_main.cc',
      ],
    },  # target turnserver
    {
      'target_name': 'peerconnection_server',
      'type': 'executable',
      'sources': [
        'examples/peerconnection/server/data_socket.cc',
        'examples/peerconnection/server/data_socket.h',
        'examples/peerconnection/server/main.cc',
        'examples/peerconnection/server/peer_channel.cc',
        'examples/peerconnection/server/peer_channel.h',
        'examples/peerconnection/server/utils.cc',
        'examples/peerconnection/server/utils.h',
      ],
      'dependencies': [
        '<(webrtc_root)/common.gyp:webrtc_common',
        'libjingle.gyp:libjingle',
      ],
      # TODO(ronghuawu): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4309, ],
    }, # target peerconnection_server
  ],
  'conditions': [
    ['OS=="linux" or OS=="win"', {
      'targets': [
        {
          'target_name': 'peerconnection_client',
          'type': 'executable',
          'sources': [
            'examples/peerconnection/client/conductor.cc',
            'examples/peerconnection/client/conductor.h',
            'examples/peerconnection/client/defaults.cc',
            'examples/peerconnection/client/defaults.h',
            'examples/peerconnection/client/peer_connection_client.cc',
            'examples/peerconnection/client/peer_connection_client.h',
          ],
          'dependencies': [
            '<(DEPTH)/third_party/jsoncpp/jsoncpp.gyp:jsoncpp',
            'libjingle.gyp:libjingle_peerconnection',
            '<@(libjingle_tests_additional_deps)',
          ],
          'conditions': [
            # TODO(ronghuawu): Move these files to a win/ directory then they
            # can be excluded automatically.
            ['OS=="win"', {
              'sources': [
                'examples/peerconnection/client/flagdefs.h',
                'examples/peerconnection/client/main.cc',
                'examples/peerconnection/client/main_wnd.cc',
                'examples/peerconnection/client/main_wnd.h',
              ],
              'msvs_settings': {
                'VCLinkerTool': {
                 'SubSystem': '2',  # Windows
                },
              },
            }],  # OS=="win"
            ['OS=="linux"', {
              'sources': [
                'examples/peerconnection/client/linux/main.cc',
                'examples/peerconnection/client/linux/main_wnd.cc',
                'examples/peerconnection/client/linux/main_wnd.h',
              ],
              'cflags': [
                '<!@(pkg-config --cflags glib-2.0 gobject-2.0 gtk+-2.0)',
              ],
              'link_settings': {
                'ldflags': [
                  '<!@(pkg-config --libs-only-L --libs-only-other glib-2.0'
                      ' gobject-2.0 gthread-2.0 gtk+-2.0)',
                ],
                'libraries': [
                  '<!@(pkg-config --libs-only-l glib-2.0 gobject-2.0'
                      ' gthread-2.0 gtk+-2.0)',
                  '-lX11',
                  '-lXcomposite',
                  '-lXext',
                  '-lXrender',
                ],
              },
            }],  # OS=="linux"
          ],  # conditions
        },  # target peerconnection_client
      ], # targets
    }],  # OS=="linux" or OS=="win"

    ['OS=="ios" or (OS=="mac" and target_arch!="ia32" and mac_sdk>="10.8")', {
      'targets': [
        { 'target_name': 'apprtc_signaling',
          'type': 'static_library',
          'dependencies': [
            'libjingle.gyp:libjingle_peerconnection_objc',
            'socketrocket',
          ],
          'sources': [
            'examples/objc/AppRTCDemo/ARDAppClient.h',
            'examples/objc/AppRTCDemo/ARDAppClient.m',
            'examples/objc/AppRTCDemo/ARDAppClient+Internal.h',
            'examples/objc/AppRTCDemo/ARDAppEngineClient.h',
            'examples/objc/AppRTCDemo/ARDAppEngineClient.m',
            'examples/objc/AppRTCDemo/ARDCEODTURNClient.h',
            'examples/objc/AppRTCDemo/ARDCEODTURNClient.m',
            'examples/objc/AppRTCDemo/ARDJoinResponse.h',
            'examples/objc/AppRTCDemo/ARDJoinResponse.m',
            'examples/objc/AppRTCDemo/ARDJoinResponse+Internal.h',
            'examples/objc/AppRTCDemo/ARDMessageResponse.h',
            'examples/objc/AppRTCDemo/ARDMessageResponse.m',
            'examples/objc/AppRTCDemo/ARDMessageResponse+Internal.h',
            'examples/objc/AppRTCDemo/ARDRoomServerClient.h',
            'examples/objc/AppRTCDemo/ARDSignalingChannel.h',
            'examples/objc/AppRTCDemo/ARDSignalingMessage.h',
            'examples/objc/AppRTCDemo/ARDSignalingMessage.m',
            'examples/objc/AppRTCDemo/ARDTURNClient.h',
            'examples/objc/AppRTCDemo/ARDUtilities.h',
            'examples/objc/AppRTCDemo/ARDUtilities.m',
            'examples/objc/AppRTCDemo/ARDWebSocketChannel.h',
            'examples/objc/AppRTCDemo/ARDWebSocketChannel.m',
            'examples/objc/AppRTCDemo/RTCICECandidate+JSON.h',
            'examples/objc/AppRTCDemo/RTCICECandidate+JSON.m',
            'examples/objc/AppRTCDemo/RTCICEServer+JSON.h',
            'examples/objc/AppRTCDemo/RTCICEServer+JSON.m',
            'examples/objc/AppRTCDemo/RTCMediaConstraints+JSON.h',
            'examples/objc/AppRTCDemo/RTCMediaConstraints+JSON.m',
            'examples/objc/AppRTCDemo/RTCSessionDescription+JSON.h',
            'examples/objc/AppRTCDemo/RTCSessionDescription+JSON.m',
          ],
          'include_dirs': [
            'examples/objc/AppRTCDemo',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'examples/objc/AppRTCDemo',
            ],
          },
          'export_dependent_settings': [
            'libjingle.gyp:libjingle_peerconnection_objc',
          ],
          'conditions': [
            ['OS=="mac"', {
              'xcode_settings': {
                'MACOSX_DEPLOYMENT_TARGET' : '10.8',
              },
            }],
          ],
        },
        {
          'target_name': 'AppRTCDemo',
          'type': 'executable',
          'product_name': 'AppRTCDemo',
          'mac_bundle': 1,
          'dependencies': [
            'apprtc_signaling',
          ],
          'conditions': [
            ['OS=="ios"', {
              'mac_bundle_resources': [
                'examples/objc/AppRTCDemo/ios/resources/Default-568h.png',
                'examples/objc/AppRTCDemo/ios/resources/Roboto-Regular.ttf',
                'examples/objc/AppRTCDemo/ios/resources/ic_call_end_black_24dp.png',
                'examples/objc/AppRTCDemo/ios/resources/ic_call_end_black_24dp@2x.png',
                'examples/objc/AppRTCDemo/ios/resources/ic_clear_black_24dp.png',
                'examples/objc/AppRTCDemo/ios/resources/ic_clear_black_24dp@2x.png',
                'examples/objc/Icon.png',
              ],
              'sources': [
                'examples/objc/AppRTCDemo/ios/ARDAppDelegate.h',
                'examples/objc/AppRTCDemo/ios/ARDAppDelegate.m',
                'examples/objc/AppRTCDemo/ios/ARDMainView.h',
                'examples/objc/AppRTCDemo/ios/ARDMainView.m',
                'examples/objc/AppRTCDemo/ios/ARDMainViewController.h',
                'examples/objc/AppRTCDemo/ios/ARDMainViewController.m',
                'examples/objc/AppRTCDemo/ios/ARDVideoCallView.h',
                'examples/objc/AppRTCDemo/ios/ARDVideoCallView.m',
                'examples/objc/AppRTCDemo/ios/ARDVideoCallViewController.h',
                'examples/objc/AppRTCDemo/ios/ARDVideoCallViewController.m',
                'examples/objc/AppRTCDemo/ios/AppRTCDemo-Prefix.pch',
                'examples/objc/AppRTCDemo/ios/UIImage+ARDUtilities.h',
                'examples/objc/AppRTCDemo/ios/UIImage+ARDUtilities.m',
                'examples/objc/AppRTCDemo/ios/main.m',
              ],
              'xcode_settings': {
                'INFOPLIST_FILE': 'examples/objc/AppRTCDemo/ios/Info.plist',
              },
            }],
            ['OS=="mac"', {
              'sources': [
                'examples/objc/AppRTCDemo/mac/APPRTCAppDelegate.h',
                'examples/objc/AppRTCDemo/mac/APPRTCAppDelegate.m',
                'examples/objc/AppRTCDemo/mac/APPRTCViewController.h',
                'examples/objc/AppRTCDemo/mac/APPRTCViewController.m',
                'examples/objc/AppRTCDemo/mac/main.m',
              ],
              'xcode_settings': {
                'CLANG_WARN_OBJC_MISSING_PROPERTY_SYNTHESIS': 'NO',
                'INFOPLIST_FILE': 'examples/objc/AppRTCDemo/mac/Info.plist',
                'MACOSX_DEPLOYMENT_TARGET' : '10.8',
                'OTHER_LDFLAGS': [
                  '-framework AVFoundation',
                ],
              },
            }],
            ['target_arch=="ia32"', {
              'dependencies' : [
                '<(DEPTH)/testing/iossim/iossim.gyp:iossim#host',
              ],
            }],
          ],
        },  # target AppRTCDemo
        {
          # TODO(tkchin): move this into the real third party location and
          # have it mirrored on chrome infra.
          'target_name': 'socketrocket',
          'type': 'static_library',
          'sources': [
            'examples/objc/AppRTCDemo/third_party/SocketRocket/SRWebSocket.h',
            'examples/objc/AppRTCDemo/third_party/SocketRocket/SRWebSocket.m',
          ],
          'conditions': [
            ['OS=="mac"', {
              'xcode_settings': {
                # SocketRocket autosynthesizes some properties. Disable the
                # warning so we can compile successfully.
                'CLANG_WARN_OBJC_MISSING_PROPERTY_SYNTHESIS': 'NO',
                'MACOSX_DEPLOYMENT_TARGET' : '10.8',
              },
            }],
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'examples/objc/AppRTCDemo/third_party/SocketRocket',
            ],
          },
          'xcode_settings': {
            'CLANG_ENABLE_OBJC_ARC': 'YES',
            'WARNING_CFLAGS': [
              '-Wno-deprecated-declarations',
            ],
          },
          'link_settings': {
            'xcode_settings': {
              'OTHER_LDFLAGS': [
                '-framework CFNetwork',
              ],
            },
            'libraries': [
              '$(SDKROOT)/usr/lib/libicucore.dylib',
            ],
          }
        },  # target socketrocket
      ],  # targets
    }],  # OS=="ios" or (OS=="mac" and target_arch!="ia32" and mac_sdk>="10.8")

    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'AppRTCDemo',
          'type': 'none',
          'variables': {
            'apk_name': 'AppRTCDemo',
            'java_in_dir': 'examples/android',
            'resource_dir': 'examples/android/res',
            'input_jars_paths': [
              'examples/android/third_party/autobanh/autobanh.jar',
             ],
            'library_dexed_jars_paths': [
              'examples/android/third_party/autobanh/autobanh.jar',
             ],
            'native_lib_target': 'libjingle_peerconnection_so',
          },
          'dependencies': [
            'libjingle.gyp:libjingle_peerconnection_java',
          ],
          'includes': [ '../build/java_apk.gypi' ],
        },  # target AppRTCDemo
      ],  # targets
    }],  # OS=="android"

    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'AppRTCDemoTest',
          'type': 'none',
          'dependencies': [
            'AppRTCDemo',
          ],
          'variables': {
            'apk_name': 'AppRTCDemoTest',
            'java_in_dir': 'examples/androidtests',
            'additional_src_dirs': [
              'examples/android',
            ],
            'input_jars_paths': [
              'examples/android/third_party/autobanh/autobanh.jar',
             ],
            'is_test_apk': 1,
          },
          'includes': [ '../build/java_apk.gypi' ],
        },
      ],  # targets
    }],  # OS=="android"
  ],
}
