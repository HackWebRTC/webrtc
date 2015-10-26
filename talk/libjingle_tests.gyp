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
  'targets': [
    {
      'target_name': 'libjingle_unittest_main',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/base/base_tests.gyp:rtc_base_tests_utils',
        '<@(libjingle_tests_additional_deps)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(libyuv_dir)/include',
          '<(DEPTH)/testing/gtest/include',
          '<(DEPTH)/testing/gtest',
        ],
      },
      'conditions': [
        ['build_libyuv==1', {
          'dependencies': ['<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv',],
        }],
      ],
      'include_dirs': [
         '<(DEPTH)/testing/gtest/include',
         '<(DEPTH)/testing/gtest',
       ],
      'sources': [
        'media/base/fakecapturemanager.h',
        'media/base/fakemediaengine.h',
        'media/base/fakenetworkinterface.h',
        'media/base/fakertp.h',
        'media/base/fakevideocapturer.h',
        'media/base/fakevideorenderer.h',
        'media/base/testutils.cc',
        'media/base/testutils.h',
        'media/devices/fakedevicemanager.h',
        'media/webrtc/fakewebrtccall.cc',
        'media/webrtc/fakewebrtccall.h',
        'media/webrtc/fakewebrtccommon.h',
        'media/webrtc/fakewebrtcdeviceinfo.h',
        'media/webrtc/fakewebrtcvcmfactory.h',
        'media/webrtc/fakewebrtcvideocapturemodule.h',
        'media/webrtc/fakewebrtcvideoengine.h',
        'media/webrtc/fakewebrtcvoiceengine.h',
      ],
    },  # target libjingle_unittest_main
    {
      'target_name': 'libjingle_media_unittest',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/base/base_tests.gyp:rtc_base_tests_utils',
        'libjingle.gyp:libjingle_media',
        'libjingle_unittest_main',
      ],
      'sources': [
        'media/base/capturemanager_unittest.cc',
        'media/base/codec_unittest.cc',
        'media/base/rtpdataengine_unittest.cc',
        'media/base/rtpdump_unittest.cc',
        'media/base/rtputils_unittest.cc',
        'media/base/streamparams_unittest.cc',
        'media/base/testutils.cc',
        'media/base/testutils.h',
        'media/base/videoadapter_unittest.cc',
        'media/base/videocapturer_unittest.cc',
        'media/base/videocommon_unittest.cc',
        'media/base/videoengine_unittest.h',
        'media/devices/dummydevicemanager_unittest.cc',
        'media/devices/filevideocapturer_unittest.cc',
        'media/sctp/sctpdataengine_unittest.cc',
        'media/webrtc/simulcast_unittest.cc',
        'media/webrtc/webrtcvideocapturer_unittest.cc',
        'media/base/videoframe_unittest.h',
        'media/webrtc/webrtcvideoframe_unittest.cc',
        'media/webrtc/webrtcvideoframefactory_unittest.cc',

        # Disabled because some tests fail.
        # TODO(ronghuawu): Reenable these tests.
        # 'media/devices/devicemanager_unittest.cc',
        'media/webrtc/webrtcvideoengine2_unittest.cc',
        'media/webrtc/webrtcvoiceengine_unittest.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'conditions': [
            ['use_openssl==0', {
              'dependencies': [
                '<(DEPTH)/net/third_party/nss/ssl.gyp:libssl',
                '<(DEPTH)/third_party/nss/nss.gyp:nspr',
                '<(DEPTH)/third_party/nss/nss.gyp:nss',
              ],
            }],
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalDependencies': [
                # TODO(ronghuawu): Since we've included strmiids in
                # libjingle_media target, we shouldn't need this here.
                # Find out why it doesn't work without this.
                'strmiids.lib',
              ],
            },
          },
        }],
        ['OS=="ios"', {
          'sources!': [
            'media/sctp/sctpdataengine_unittest.cc',
          ],
        }],
      ],
    },  # target libjingle_media_unittest
    {
      'target_name': 'libjingle_p2p_unittest',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/base/base_tests.gyp:rtc_base_tests_utils',
        'libjingle.gyp:libjingle',
        'libjingle.gyp:libjingle_peerconnection',
        'libjingle.gyp:libjingle_p2p',
        'libjingle_unittest_main',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/libsrtp/srtp',
      ],
      'sources': [
        'session/media/bundlefilter_unittest.cc',
        'session/media/channel_unittest.cc',
        'session/media/channelmanager_unittest.cc',
        'session/media/currentspeakermonitor_unittest.cc',
        'session/media/mediasession_unittest.cc',
        'session/media/rtcpmuxfilter_unittest.cc',
        'session/media/srtpfilter_unittest.cc',
      ],
      'conditions': [
        ['build_libsrtp==1', {
          'dependencies': [
            '<(DEPTH)/third_party/libsrtp/libsrtp.gyp:libsrtp',
          ],
        }],
        ['OS=="win"', {
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalDependencies': [
                'strmiids.lib',
              ],
            },
          },
        }],
      ],
    },  # target libjingle_p2p_unittest
    {
      'target_name': 'libjingle_peerconnection_unittest',
      'type': 'executable',
      'dependencies': [
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(webrtc_root)/base/base_tests.gyp:rtc_base_tests_utils',
        '<(webrtc_root)/common.gyp:webrtc_common',
        'libjingle.gyp:libjingle',
        'libjingle.gyp:libjingle_p2p',
        'libjingle.gyp:libjingle_peerconnection',
        'libjingle_unittest_main',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/testing/gmock/include',
        ],
      },
      'sources': [
        'app/webrtc/datachannel_unittest.cc',
        'app/webrtc/dtlsidentitystore_unittest.cc',
        'app/webrtc/dtmfsender_unittest.cc',
        'app/webrtc/fakemetricsobserver.cc',
        'app/webrtc/fakemetricsobserver.h',
        'app/webrtc/jsepsessiondescription_unittest.cc',
        'app/webrtc/localaudiosource_unittest.cc',
        'app/webrtc/mediastream_unittest.cc',
        'app/webrtc/peerconnection_unittest.cc',
        'app/webrtc/peerconnectionendtoend_unittest.cc',
        'app/webrtc/peerconnectionfactory_unittest.cc',
        'app/webrtc/peerconnectioninterface_unittest.cc',
        # 'app/webrtc/peerconnectionproxy_unittest.cc',
        'app/webrtc/remotevideocapturer_unittest.cc',
        'app/webrtc/rtpsenderreceiver_unittest.cc',
        'app/webrtc/sctputils.cc',
        'app/webrtc/statscollector_unittest.cc',
        'app/webrtc/test/fakeaudiocapturemodule.cc',
        'app/webrtc/test/fakeaudiocapturemodule.h',
        'app/webrtc/test/fakeaudiocapturemodule_unittest.cc',
        'app/webrtc/test/fakeconstraints.h',
        'app/webrtc/test/fakedatachannelprovider.h',
        'app/webrtc/test/fakedtlsidentitystore.h',
        'app/webrtc/test/fakemediastreamsignaling.h',
        'app/webrtc/test/fakeperiodicvideocapturer.h',
        'app/webrtc/test/fakevideotrackrenderer.h',
        'app/webrtc/test/mockpeerconnectionobservers.h',
        'app/webrtc/test/peerconnectiontestwrapper.h',
        'app/webrtc/test/peerconnectiontestwrapper.cc',
        'app/webrtc/test/testsdpstrings.h',
        'app/webrtc/videosource_unittest.cc',
        'app/webrtc/videotrack_unittest.cc',
        'app/webrtc/webrtcsdp_unittest.cc',
        'app/webrtc/webrtcsession_unittest.cc',
      ],
      'conditions': [
        ['OS=="android"', {
          # We want gmock features that use tr1::tuple, but we currently
          # don't support the variadic templates used by libstdc++'s
          # implementation. gmock supports this scenario by providing its
          # own implementation but we must opt in to it.
          'defines': [
            'GTEST_USE_OWN_TR1_TUPLE=1',
            # GTEST_USE_OWN_TR1_TUPLE only works if GTEST_HAS_TR1_TUPLE is set.
            # gmock r625 made it so that GTEST_HAS_TR1_TUPLE is set to 0
            # automatically on android, so it has to be set explicitly here.
            'GTEST_HAS_TR1_TUPLE=1',
           ],
        }],
      ],
    },  # target libjingle_peerconnection_unittest
  ],
  'conditions': [
    ['OS=="linux"', {
      'variables': {
        'junit_jar': '<(DEPTH)/third_party/junit-jar/junit-4.11.jar',
      },
      'targets': [
        {
          'target_name': 'libjingle_peerconnection_test_jar',
          'type': 'none',
          'dependencies': [
            'libjingle.gyp:libjingle_peerconnection_jar',
          ],
          'actions': [
            {
              'variables': {
                'java_src_dir': 'app/webrtc/javatests/src',
                'java_files': [
                  'app/webrtc/java/testcommon/src/org/webrtc/PeerConnectionTest.java',
                  'app/webrtc/javatests/src/org/webrtc/PeerConnectionTestJava.java',
                ],
              },
              'action_name': 'create_jar',
              'inputs': [
                'build/build_jar.sh',
                '<@(java_files)',
                '<(PRODUCT_DIR)/libjingle_peerconnection.jar',
                '<(PRODUCT_DIR)/lib/libjingle_peerconnection_so.so',
                '<(junit_jar)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/libjingle_peerconnection_test.jar',
              ],
              'action': [
                'build/build_jar.sh', '<(java_home)', '<@(_outputs)',
                '<(INTERMEDIATE_DIR)',
                '<(java_src_dir):<(PRODUCT_DIR)/libjingle_peerconnection.jar:<(junit_jar)',
                '<@(java_files)'
              ],
            },
          ],
        },
        {
          'target_name': 'libjingle_peerconnection_java_unittest',
          'type': 'none',
          'actions': [
            {
              'action_name': 'copy libjingle_peerconnection_java_unittest',
              'inputs': [
                'app/webrtc/javatests/libjingle_peerconnection_java_unittest.sh',
                '<(PRODUCT_DIR)/libjingle_peerconnection_test_jar',
                '<(junit_jar)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/libjingle_peerconnection_java_unittest',
              ],
              'action': [
                'bash', '-c',
                'rm -f <(PRODUCT_DIR)/libjingle_peerconnection_java_unittest && '
                'sed -e "s@GYP_JAVA_HOME@<(java_home)@" '
                '< app/webrtc/javatests/libjingle_peerconnection_java_unittest.sh '
                '> <(PRODUCT_DIR)/libjingle_peerconnection_java_unittest && '
                'cp <(junit_jar) <(PRODUCT_DIR) && '
                'chmod u+x <(PRODUCT_DIR)/libjingle_peerconnection_java_unittest'
              ],
            },
          ],
        },
      ],
    }],
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'libjingle_peerconnection_android_unittest',
          'type': 'none',
          'dependencies': [
            'libjingle.gyp:libjingle_peerconnection_java',
          ],
          'variables': {
            'apk_name': 'libjingle_peerconnection_android_unittest',
            'java_in_dir': 'app/webrtc/androidtests',
            'resource_dir': 'app/webrtc/androidtests/res',
            'additional_src_dirs': ['app/webrtc/java/testcommon'],
            'native_lib_target': 'libjingle_peerconnection_so',
            'is_test_apk': 1,
          },
          'includes': [ '../build/java_apk.gypi' ],
        },
      ],  # targets
    }],  # OS=="android"
    ['OS=="ios" or (OS=="mac" and target_arch!="ia32" and mac_sdk>="10.7")', {
      # The >=10.7 above is required to make ARC link cleanly (e.g. as
      # opposed to _compile_ cleanly, which the library under test
      # does just fine on 10.6 too).
      'targets': [
        {
          'target_name': 'libjingle_peerconnection_objc_test',
          'type': 'executable',
          'includes': [ 'build/objc_app.gypi' ],
          'dependencies': [
            '<(webrtc_root)/base/base_tests.gyp:rtc_base_tests_utils',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:field_trial_default',
            'libjingle.gyp:libjingle_peerconnection_objc',
          ],
          'sources': [
            'app/webrtc/objctests/RTCPeerConnectionSyncObserver.h',
            'app/webrtc/objctests/RTCPeerConnectionSyncObserver.m',
            'app/webrtc/objctests/RTCPeerConnectionTest.mm',
            'app/webrtc/objctests/RTCSessionDescriptionSyncObserver.h',
            'app/webrtc/objctests/RTCSessionDescriptionSyncObserver.m',
            # TODO(fischman): figure out if this works for ios or if it
            # needs a GUI driver.
            'app/webrtc/objctests/mac/main.mm',
          ],
          'conditions': [
            ['OS=="mac"', {
              'xcode_settings': {
                # Need to build against 10.7 framework for full ARC support
                # on OSX.
                'MACOSX_DEPLOYMENT_TARGET' : '10.7',
                # common.gypi enables this for mac but we want this to be
                # disabled like it is for ios.
                'CLANG_WARN_OBJC_MISSING_PROPERTY_SYNTHESIS': 'NO',
              },
            }],
          ],
        },  # target libjingle_peerconnection_objc_test
        {
          'target_name': 'apprtc_signaling_gunit_test',
          'type': 'executable',
          'includes': [ 'build/objc_app.gypi' ],
          'dependencies': [
            '<(webrtc_root)/base/base_tests.gyp:rtc_base_tests_utils',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:field_trial_default',
            '<(DEPTH)/third_party/ocmock/ocmock.gyp:ocmock',
            '<(webrtc_root)/libjingle_examples.gyp:apprtc_signaling',
          ],
          'sources': [
            'app/webrtc/objctests/mac/main.mm',
            '<(webrtc_root)/examples/objc/AppRTCDemo/tests/ARDAppClientTest.mm',
          ],
          'conditions': [
            ['OS=="mac"', {
              'xcode_settings': {
                'MACOSX_DEPLOYMENT_TARGET' : '10.8',
              },
            }],
          ],
        },  # target apprtc_signaling_gunit_test
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'libjingle_media_unittest_run',
          'type': 'none',
          'dependencies': [
            'libjingle_media_unittest',
          ],
          'includes': [
            'build/isolate.gypi',
          ],
          'sources': [
            'libjingle_media_unittest.isolate',
          ],
        },
        {
          'target_name': 'libjingle_p2p_unittest_run',
          'type': 'none',
          'dependencies': [
            'libjingle_p2p_unittest',
          ],
          'includes': [
            'build/isolate.gypi',
          ],
          'sources': [
            'libjingle_p2p_unittest.isolate',
          ],
        },
        {
          'target_name': 'libjingle_peerconnection_unittest_run',
          'type': 'none',
          'dependencies': [
            'libjingle_peerconnection_unittest',
          ],
          'includes': [
            'build/isolate.gypi',
          ],
          'sources': [
            'libjingle_peerconnection_unittest.isolate',
          ],
        },
      ],
    }],
  ],
}
