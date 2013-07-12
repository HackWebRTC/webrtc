#
# libjingle
# Copyright 2012, Google Inc.
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
#

{
  'includes': ['build/common.gypi'],
  'targets': [
    {
      # TODO(ronghuawu): Use gtest.gyp from chromium.
      'target_name': 'gunit',
      'type': 'static_library',
      'sources': [
        '<(DEPTH)/testing/gtest/src/gtest-all.cc',
      ],
      'include_dirs': [
        '<(DEPTH)/testing/gtest/include',
        '<(DEPTH)/testing/gtest',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/testing/gtest/include',
        ],
      },
      'conditions': [
        ['OS=="android"', {
          'include_dirs': [
            '<(android_ndk_include)',
          ]
        }],
      ],
    },  # target gunit
    {
      'target_name': 'libjingle_unittest_main',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv',
        'gunit',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/libyuv/include',
        ],
      },
      'sources': [
        'base/unittest_main.cc',
        # Also use this as a convenient dumping ground for misc files that are
        # included by multiple targets below.
        'base/fakecpumonitor.h',
        'base/fakenetwork.h',
        'base/fakesslidentity.h',
        'base/faketaskrunner.h',
        'base/gunit.h',
        'base/testbase64.h',
        'base/testechoserver.h',
        'base/win32toolhelp.h',
        'media/base/fakecapturemanager.h',
        'media/base/fakemediaengine.h',
        'media/base/fakemediaprocessor.h',
        'media/base/fakenetworkinterface.h',
        'media/base/fakertp.h',
        'media/base/fakevideocapturer.h',
        'media/base/fakevideorenderer.h',
        'media/base/nullvideoframe.h',
        'media/base/nullvideorenderer.h',
        'media/base/testutils.cc',
        'media/base/testutils.h',
        'media/devices/fakedevicemanager.h',
        'media/webrtc/fakewebrtccommon.h',
        'media/webrtc/fakewebrtcdeviceinfo.h',
        'media/webrtc/fakewebrtcvcmfactory.h',
        'media/webrtc/fakewebrtcvideocapturemodule.h',
        'media/webrtc/fakewebrtcvideoengine.h',
        'media/webrtc/fakewebrtcvoiceengine.h',
      ],
    },  # target libjingle_unittest_main
    {
      'target_name': 'libjingle_unittest',
      'type': 'executable',
      'dependencies': [
        'gunit',
        'libjingle.gyp:libjingle',
        'libjingle_unittest_main',
      ],
      'sources': [
        'base/asynchttprequest_unittest.cc',
        'base/atomicops_unittest.cc',
        'base/autodetectproxy_unittest.cc',
        'base/bandwidthsmoother_unittest.cc',
        'base/base64_unittest.cc',
        'base/basictypes_unittest.cc',
        'base/bind_unittest.cc',
        'base/buffer_unittest.cc',
        'base/bytebuffer_unittest.cc',
        'base/byteorder_unittest.cc',
        'base/cpumonitor_unittest.cc',
        'base/crc32_unittest.cc',
        'base/event_unittest.cc',
        'base/filelock_unittest.cc',
        'base/fileutils_unittest.cc',
        'base/helpers_unittest.cc',
        'base/host_unittest.cc',
        'base/httpbase_unittest.cc',
        'base/httpcommon_unittest.cc',
        'base/httpserver_unittest.cc',
        'base/ipaddress_unittest.cc',
        'base/logging_unittest.cc',
        'base/md5digest_unittest.cc',
        'base/messagedigest_unittest.cc',
        'base/messagequeue_unittest.cc',
        'base/multipart_unittest.cc',
        'base/nat_unittest.cc',
        'base/network_unittest.cc',
        'base/nullsocketserver_unittest.cc',
        'base/optionsfile_unittest.cc',
        'base/pathutils_unittest.cc',
        'base/physicalsocketserver_unittest.cc',
        'base/profiler_unittest.cc',
        'base/proxy_unittest.cc',
        'base/proxydetect_unittest.cc',
        'base/ratelimiter_unittest.cc',
        'base/ratetracker_unittest.cc',
        'base/referencecountedsingletonfactory_unittest.cc',
        'base/rollingaccumulator_unittest.cc',
        'base/sha1digest_unittest.cc',
        'base/sharedexclusivelock_unittest.cc',
        'base/signalthread_unittest.cc',
        'base/sigslot_unittest.cc',
        'base/socket_unittest.cc',
        'base/socket_unittest.h',
        'base/socketaddress_unittest.cc',
        'base/stream_unittest.cc',
        'base/stringencode_unittest.cc',
        'base/stringutils_unittest.cc',
        # TODO(ronghuawu): Reenable this test.
        # 'base/systeminfo_unittest.cc',
        'base/task_unittest.cc',
        'base/testclient_unittest.cc',
        'base/thread_unittest.cc',
        'base/timeutils_unittest.cc',
        'base/urlencode_unittest.cc',
        'base/versionparsing_unittest.cc',
        'base/virtualsocket_unittest.cc',
        # TODO(ronghuawu): Reenable this test.
        # 'base/windowpicker_unittest.cc',
        'xmllite/qname_unittest.cc',
        'xmllite/xmlbuilder_unittest.cc',
        'xmllite/xmlelement_unittest.cc',
        'xmllite/xmlnsstack_unittest.cc',
        'xmllite/xmlparser_unittest.cc',
        'xmllite/xmlprinter_unittest.cc',
        'xmpp/fakexmppclient.h',
        'xmpp/hangoutpubsubclient_unittest.cc',
        'xmpp/jid_unittest.cc',
        'xmpp/mucroomconfigtask_unittest.cc',
        'xmpp/mucroomdiscoverytask_unittest.cc',
        'xmpp/mucroomlookuptask_unittest.cc',
        'xmpp/mucroomuniquehangoutidtask_unittest.cc',
        'xmpp/pingtask_unittest.cc',
        'xmpp/pubsubclient_unittest.cc',
        'xmpp/pubsubtasks_unittest.cc',
        'xmpp/util_unittest.cc',
        'xmpp/util_unittest.h',
        'xmpp/xmppengine_unittest.cc',
        'xmpp/xmpplogintask_unittest.cc',
        'xmpp/xmppstanzaparser_unittest.cc',
      ],  # sources
      'conditions': [
        ['OS=="linux"', {
          'sources': [
            'base/latebindingsymboltable_unittest.cc',
            # TODO(ronghuawu): Reenable this test.
            # 'base/linux_unittest.cc',
            'base/linuxfdwalk_unittest.cc',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            'base/win32_unittest.cc',
            'base/win32regkey_unittest.cc',
            'base/win32socketserver_unittest.cc',
            'base/win32toolhelp_unittest.cc',
            'base/win32window_unittest.cc',
            'base/win32windowpicker_unittest.cc',
            'base/winfirewall_unittest.cc',
          ],
          'sources!': [
            # TODO(ronghuawu): Fix TestUdpReadyToSendIPv6 on windows bot
            # then reenable these tests.
            'base/physicalsocketserver_unittest.cc',
            'base/socket_unittest.cc',
            'base/win32socketserver_unittest.cc',
            'base/win32windowpicker_unittest.cc',
          ],
        }],
        ['OS=="mac"', {
          'sources': [
            'base/macsocketserver_unittest.cc',
            'base/macutils_unittest.cc',
            'base/macwindowpicker_unittest.cc',
          ],
        }],
        ['os_posix==1', {
          'sources': [
            'base/sslidentity_unittest.cc',
            # TODO(ronghuawu): reenable once fixed on build bots.
            # 'base/sslstreamadapter_unittest.cc',
          ],
        }],
      ],  # conditions
    },  # target libjingle_unittest
    {
      'target_name': 'libjingle_sound_unittest',
      'type': 'executable',
      'dependencies': [
        'gunit',
        'libjingle.gyp:libjingle_sound',
        'libjingle_unittest_main',
      ],
      'sources': [
        'sound/automaticallychosensoundsystem_unittest.cc',
      ],
    },  # target libjingle_sound_unittest
    {
      'target_name': 'libjingle_media_unittest',
      'type': 'executable',
      'dependencies': [
        'gunit',
        'libjingle.gyp:libjingle_media',
        'libjingle_unittest_main',
      ],
      # TODO(ronghuawu): Avoid the copies.
      # https://code.google.com/p/libjingle/issues/detail?id=398
      'copies': [
        {
          'destination': '<(DEPTH)/../talk/media/testdata',
          'files': [
            'media/testdata/1.frame_plus_1.byte',
            'media/testdata/captured-320x240-2s-48.frames',
            'media/testdata/h264-svc-99-640x360.rtpdump',
            'media/testdata/video.rtpdump',
            'media/testdata/voice.rtpdump',
          ],
        },
      ],
      'sources': [
        # TODO(ronghuawu): Reenable this test.
        # 'media/base/capturemanager_unittest.cc',
        'media/base/codec_unittest.cc',
        'media/base/filemediaengine_unittest.cc',
        'media/base/rtpdataengine_unittest.cc',
        'media/base/rtpdump_unittest.cc',
        'media/base/rtputils_unittest.cc',
        'media/base/testutils.cc',
        'media/base/testutils.h',
        'media/base/videocapturer_unittest.cc',
        'media/base/videocommon_unittest.cc',
        'media/base/videoengine_unittest.h',
        'media/devices/dummydevicemanager_unittest.cc',
        'media/devices/filevideocapturer_unittest.cc',
        'media/webrtc/webrtcpassthroughrender_unittest.cc',
        'media/webrtc/webrtcvideocapturer_unittest.cc',
        # Omitted because depends on non-open-source testdata files.
        # 'media/base/videoframe_unittest.h',
        # 'media/webrtc/webrtcvideoframe_unittest.cc',

        # Disabled because some tests fail.
        # TODO(ronghuawu): Reenable these tests.
        # 'media/devices/devicemanager_unittest.cc',
        # 'media/webrtc/webrtcvideoengine_unittest.cc',
        # 'media/webrtc/webrtcvoiceengine_unittest.cc',
      ],
      'conditions': [
        ['OS=="win"', {
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
      ],
    },  # target libjingle_media_unittest
    {
      'target_name': 'libjingle_p2p_unittest',
      'type': 'executable',
      'dependencies': [
        '<(DEPTH)/third_party/libsrtp/libsrtp.gyp:libsrtp',
        'gunit',
        'libjingle.gyp:libjingle',
        'libjingle.gyp:libjingle_p2p',
        'libjingle_unittest_main',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/libsrtp/srtp',
      ],
      'sources': [
        'p2p/base/dtlstransportchannel_unittest.cc',
        'p2p/base/fakesession.h',
        'p2p/base/p2ptransportchannel_unittest.cc',
        'p2p/base/port_unittest.cc',
        'p2p/base/portallocatorsessionproxy_unittest.cc',
        'p2p/base/pseudotcp_unittest.cc',
        'p2p/base/relayport_unittest.cc',
        'p2p/base/relayserver_unittest.cc',
        'p2p/base/session_unittest.cc',
        'p2p/base/stun_unittest.cc',
        'p2p/base/stunport_unittest.cc',
        'p2p/base/stunrequest_unittest.cc',
        'p2p/base/stunserver_unittest.cc',
        'p2p/base/testrelayserver.h',
        'p2p/base/teststunserver.h',
        'p2p/base/testturnserver.h',
        'p2p/base/transport_unittest.cc',
        'p2p/base/transportdescriptionfactory_unittest.cc',
        'p2p/client/connectivitychecker_unittest.cc',
        'p2p/client/fakeportallocator.h',
        'p2p/client/portallocator_unittest.cc',
        'session/media/channel_unittest.cc',
        'session/media/channelmanager_unittest.cc',
        'session/media/currentspeakermonitor_unittest.cc',
        'session/media/mediarecorder_unittest.cc',
        'session/media/mediamessages_unittest.cc',
        'session/media/mediasession_unittest.cc',
        'session/media/mediasessionclient_unittest.cc',
        'session/media/rtcpmuxfilter_unittest.cc',
        'session/media/srtpfilter_unittest.cc',
        'session/media/ssrcmuxfilter_unittest.cc',
      ],
      'conditions': [
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
        'gunit',
        'libjingle.gyp:libjingle',
        'libjingle.gyp:libjingle_p2p',
        'libjingle.gyp:libjingle_peerconnection',
        'libjingle_unittest_main',
      ],
      # TODO(ronghuawu): Reenable below unit tests that require gmock.
      'sources': [
        'app/webrtc/dtmfsender_unittest.cc',
        'app/webrtc/jsepsessiondescription_unittest.cc',
        'app/webrtc/localaudiosource_unittest.cc',
        'app/webrtc/localvideosource_unittest.cc',
        # 'app/webrtc/mediastream_unittest.cc',
        # 'app/webrtc/mediastreamhandler_unittest.cc',
        'app/webrtc/mediastreamsignaling_unittest.cc',
        'app/webrtc/peerconnection_unittest.cc',
        'app/webrtc/peerconnectionfactory_unittest.cc',
        'app/webrtc/peerconnectioninterface_unittest.cc',
        # 'app/webrtc/peerconnectionproxy_unittest.cc',
        'app/webrtc/test/fakeaudiocapturemodule.cc',
        'app/webrtc/test/fakeaudiocapturemodule.h',
        'app/webrtc/test/fakeaudiocapturemodule_unittest.cc',
        'app/webrtc/test/fakeconstraints.h',
        'app/webrtc/test/fakeperiodicvideocapturer.h',
        'app/webrtc/test/fakevideotrackrenderer.h',
        'app/webrtc/test/mockpeerconnectionobservers.h',
        'app/webrtc/test/testsdpstrings.h',
        'app/webrtc/videotrack_unittest.cc',
        'app/webrtc/webrtcsdp_unittest.cc',
        'app/webrtc/webrtcsession_unittest.cc',
      ],
    },  # target libjingle_peerconnection_unittest
  ],
  'conditions': [
    ['OS=="linux"', {
      'targets': [
        {
          'target_name': 'libjingle_peerconnection_test_jar',
          'type': 'none',
          'actions': [
            {
              'variables': {
                'java_src_dir': 'app/webrtc/javatests/src',
                'java_files': [
                  'app/webrtc/javatests/src/org/webrtc/PeerConnectionTest.java',
                ],
              },
              'action_name': 'create_jar',
              'inputs': [
                'build/build_jar.sh',
                '<@(java_files)',
                '<(PRODUCT_DIR)/libjingle_peerconnection.jar',
                '<(DEPTH)/third_party/junit/junit-4.11.jar',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/libjingle_peerconnection_test.jar',
              ],
              'action': [
                'build/build_jar.sh', '/usr', '<@(_outputs)',
                '<(INTERMEDIATE_DIR)',
                '<(java_src_dir):<(PRODUCT_DIR)/libjingle_peerconnection.jar:<(DEPTH)/third_party/junit/junit-4.11.jar',
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
                '<(DEPTH)/third_party/junit/junit-4.11.jar',
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
                'cp <(DEPTH)/third_party/junit/junit-4.11.jar <(PRODUCT_DIR) && '
                'chmod u+x <(PRODUCT_DIR)/libjingle_peerconnection_java_unittest'
              ],
            },
          ],
        },
      ],
    }],
    ['libjingle_objc == 1', {
      'targets': [
        {
          'variables': {
            'infoplist_file': './app/webrtc/objctests/Info.plist',
          },
          'target_name': 'libjingle_peerconnection_objc_test',
          'type': 'executable',
          'mac_bundle': 1,
          'mac_bundle_resources': [
            '<(infoplist_file)',
          ],
          # The plist is listed above so that it appears in XCode's file list,
          # but we don't actually want to bundle it.
          'mac_bundle_resources!': [
            '<(infoplist_file)',
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': '<(infoplist_file)',
          },
          'dependencies': [
            'gunit',
            'libjingle.gyp:libjingle_peerconnection_objc',
          ],
          'FRAMEWORK_SEARCH_PATHS': [
            '$(inherited)',
            '$(SDKROOT)/Developer/Library/Frameworks',
            '$(DEVELOPER_LIBRARY_DIR)/Frameworks',
          ],
          'sources': [
            'app/webrtc/objctests/RTCPeerConnectionSyncObserver.h',
            'app/webrtc/objctests/RTCPeerConnectionSyncObserver.m',
            'app/webrtc/objctests/RTCPeerConnectionTest.mm',
            'app/webrtc/objctests/RTCSessionDescriptionSyncObserver.h',
            'app/webrtc/objctests/RTCSessionDescriptionSyncObserver.m',
          ],
          'include_dirs': [
            '<(DEPTH)/talk/app/webrtc/objc/public',
          ],
          'conditions': [
            [ 'OS=="mac"', {
              'sources': [
                'app/webrtc/objctests/mac/main.mm',
              ],
              'xcode_settings': {
                'CLANG_ENABLE_OBJC_ARC': 'YES',
                'CLANG_WARN_OBJC_MISSING_PROPERTY_SYNTHESIS': 'NO',
                'CLANG_LINK_OBJC_RUNTIME': 'YES',
                # build/common.gypi disables ARC by default for back-compat
                # reasons with OSX 10.6.   Enabling OBJC runtime and clearing
                # LDPLUSPLUS and CC re-enables it.  Setting deployment target to
                # 10.7 as there are no back-compat issues with ARC.
                # https://code.google.com/p/chromium/issues/detail?id=156530
                'CC': '',
                'LDPLUSPLUS': '',
                'macosx_deployment_target': '10.7',
              },
            }],
          ],
        },
      ],
    }],
  ],
}
