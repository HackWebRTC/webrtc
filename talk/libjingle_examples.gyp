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
            'examples/objc/APPRTCDemo',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'examples/objc/APPRTCDemo',
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
          'dependencies': [
            'libjingle.gyp:libjingle_peerconnection_jar',
          ],
          'actions': [
            {
              # TODO(glaznev): convert from a custom script to a standard gyp
              # apk build once chromium's apk-building gyp machinery can be used
              # (http://crbug.com/225101)
              'action_name': 'build_apprtcdemo_apk',
              'inputs' : [
                '<(PRODUCT_DIR)/libjingle_peerconnection.jar',
                '<(PRODUCT_DIR)/lib/libjingle_peerconnection_so.so',
                'examples/android/AndroidManifest.xml',
                'examples/android/README',
                'examples/android/ant.properties',
                'examples/android/third_party/autobanh/autobanh.jar',
                'examples/android/build.xml',
                'examples/android/jni/Android.mk',
                'examples/android/project.properties',
                'examples/android/res/drawable-hdpi/disconnect.png',
                'examples/android/res/drawable-hdpi/ic_action_full_screen.png',
                'examples/android/res/drawable-hdpi/ic_action_return_from_full_screen.png',
                'examples/android/res/drawable-hdpi/ic_loopback_call.png',
                'examples/android/res/drawable-hdpi/ic_launcher.png',
                'examples/android/res/drawable-ldpi/disconnect.png',
                'examples/android/res/drawable-ldpi/ic_action_full_screen.png',
                'examples/android/res/drawable-ldpi/ic_action_return_from_full_screen.png',
                'examples/android/res/drawable-ldpi/ic_loopback_call.png',
                'examples/android/res/drawable-ldpi/ic_launcher.png',
                'examples/android/res/drawable-mdpi/disconnect.png',
                'examples/android/res/drawable-mdpi/ic_action_full_screen.png',
                'examples/android/res/drawable-mdpi/ic_action_return_from_full_screen.png',
                'examples/android/res/drawable-mdpi/ic_loopback_call.png',
                'examples/android/res/drawable-mdpi/ic_launcher.png',
                'examples/android/res/drawable-xhdpi/disconnect.png',
                'examples/android/res/drawable-xhdpi/ic_action_full_screen.png',
                'examples/android/res/drawable-xhdpi/ic_action_return_from_full_screen.png',
                'examples/android/res/drawable-xhdpi/ic_loopback_call.png',
                'examples/android/res/drawable-xhdpi/ic_launcher.png',
                'examples/android/res/layout/activity_call.xml',
                'examples/android/res/layout/activity_connect.xml',
                'examples/android/res/layout/fragment_call.xml',
                'examples/android/res/menu/connect_menu.xml',
                'examples/android/res/values/arrays.xml',
                'examples/android/res/values/strings.xml',
                'examples/android/res/xml/preferences.xml',
                'examples/android/src/org/appspot/apprtc/AppRTCAudioManager.java',
                'examples/android/src/org/appspot/apprtc/AppRTCClient.java',
                'examples/android/src/org/appspot/apprtc/AppRTCProximitySensor.java',
                'examples/android/src/org/appspot/apprtc/CallActivity.java',
                'examples/android/src/org/appspot/apprtc/CallFragment.java',
                'examples/android/src/org/appspot/apprtc/ConnectActivity.java',
                'examples/android/src/org/appspot/apprtc/PeerConnectionClient.java',
                'examples/android/src/org/appspot/apprtc/RoomParametersFetcher.java',
                'examples/android/src/org/appspot/apprtc/SettingsActivity.java',
                'examples/android/src/org/appspot/apprtc/SettingsFragment.java',
                'examples/android/src/org/appspot/apprtc/UnhandledExceptionHandler.java',
                'examples/android/src/org/appspot/apprtc/WebSocketChannelClient.java',
                'examples/android/src/org/appspot/apprtc/WebSocketRTCClient.java',
                'examples/android/src/org/appspot/apprtc/util/AppRTCUtils.java',
                'examples/android/src/org/appspot/apprtc/util/AsyncHttpURLConnection.java',
                'examples/android/src/org/appspot/apprtc/util/LooperExecutor.java',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/AppRTCDemo-debug.apk',
              ],
              'variables': {
                'ant_log': '../../<(INTERMEDIATE_DIR)/ant.log', # ../.. to compensate for the cd examples/android below.
              },
              'action': [
                'bash', '-ec',
                'rm -fr <(_outputs) examples/android/{bin,libs} && '
                'mkdir -p <(INTERMEDIATE_DIR) && ' # Must happen _before_ the cd below
                'mkdir -p examples/android/libs/<(android_app_abi) && '
                'cp <(PRODUCT_DIR)/libjingle_peerconnection.jar examples/android/libs/ &&'
                'cp examples/android/third_party/autobanh/autobanh.jar examples/android/libs/ &&'
                '<(android_strip) -o examples/android/libs/<(android_app_abi)/libjingle_peerconnection_so.so  <(PRODUCT_DIR)/lib/libjingle_peerconnection_so.so &&'
                'cd examples/android && '
                '{ ANDROID_SDK_ROOT=<(android_sdk_root) '
                'ant debug > <(ant_log) 2>&1 || '
                '  { cat <(ant_log) ; exit 1; } } && '
                'cd - > /dev/null && '
                'cp examples/android/bin/AppRTCDemo-debug.apk <(_outputs)'
              ],
            },
          ],
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
          'actions': [
            {
              # TODO(glaznev): convert from a custom script to a standard gyp
              # apk build once chromium's apk-building gyp machinery can be used
              # (http://crbug.com/225101)
              'action_name': 'build_apprtcdemotest_apk',
              'inputs' : [
                'examples/androidtests/AndroidManifest.xml',
                'examples/androidtests/ant.properties',
                'examples/androidtests/build.xml',
                'examples/androidtests/project.properties',
                'examples/androidtests/src/org/appspot/apprtc/test/LooperExecutorTest.java',
                'examples/androidtests/src/org/appspot/apprtc/test/PeerConnectionClientTest.java',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/AppRTCDemoTest-debug.apk',
              ],
              'variables': {
                'ant_log': '../../<(INTERMEDIATE_DIR)/ant.log', # ../.. to compensate for the cd examples/androidtests below.
              },
              'action': [
                'bash', '-ec',
                'mkdir -p <(INTERMEDIATE_DIR) && ' # Must happen _before_ the cd below
                'cd examples/androidtests && '
                '{ ANDROID_SDK_ROOT=<(android_sdk_root) '
                'ant debug > <(ant_log) 2>&1 || '
                '  { cat <(ant_log) ; exit 1; } } && '
                'cd - > /dev/null && '
                'cp examples/androidtests/bin/AppRTCDemoTest-debug.apk <(_outputs)'
              ],
            },
          ],
        },  # target AppRTCDemoTest
      ],  # targets
    }],  # OS=="android"
  ],
}
