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
      'target_name': 'libjingle_p2p_unittest',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/base/base_tests.gyp:rtc_base_tests_utils',
        '<(webrtc_root)/webrtc.gyp:rtc_unittest_main',
        'libjingle.gyp:libjingle_peerconnection',
        'libjingle.gyp:libjingle_p2p',
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
      'target_name': 'peerconnection_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(webrtc_root)/base/base_tests.gyp:rtc_base_tests_utils',
        '<(webrtc_root)/common.gyp:webrtc_common',
        '<(webrtc_root)/webrtc.gyp:rtc_unittest_main',
        'libjingle.gyp:libjingle_p2p',
        'libjingle.gyp:libjingle_peerconnection',
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
        'app/webrtc/statscollector_unittest.cc',
        'app/webrtc/test/fakeaudiocapturemodule.cc',
        'app/webrtc/test/fakeaudiocapturemodule.h',
        'app/webrtc/test/fakeaudiocapturemodule_unittest.cc',
        'app/webrtc/test/fakeconstraints.h',
        'app/webrtc/test/fakedatachannelprovider.h',
        'app/webrtc/test/fakedtlsidentitystore.h',
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
          'sources': [
            'app/webrtc/test/androidtestinitializer.cc',
            'app/webrtc/test/androidtestinitializer.h',
          ],
          'dependencies': [
            '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
            'libjingle.gyp:libjingle_peerconnection_jni',
          ],
        }],
        ['OS=="win" and clang==1', {
          'msvs_settings': {
            'VCCLCompilerTool': {
              'AdditionalOptions': [
                # Disable warnings failing when compiling with Clang on Windows.
                # https://bugs.chromium.org/p/webrtc/issues/detail?id=5366
                '-Wno-unused-function',
              ],
            },
          },
        }],
      ],
    },  # target peerconnection_unittests
  ],
  'conditions': [
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
            'native_lib_target': 'libjingle_peerconnection_so',
            'is_test_apk': 1,
          },
          'includes': [ '../build/java_apk.gypi' ],
        },
      ],  # targets
    }],  # OS=="android"
    ['OS=="ios" or (OS=="mac" and target_arch!="ia32")', {
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
            '<(webrtc_root)/webrtc_examples.gyp:apprtc_signaling',
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
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'peerconnection_unittests_apk_target',
          'type': 'none',
          'dependencies': [
            '<(DEPTH)/webrtc/build/apk_tests.gyp:peerconnection_unittests_apk',
          ],
        },
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
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
          'target_name': 'peerconnection_unittests_run',
          'type': 'none',
          'dependencies': [
            'peerconnection_unittests',
          ],
          'includes': [
            'build/isolate.gypi',
          ],
          'sources': [
            'peerconnection_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
