# Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': ['../build/common.gypi'],
  'variables': {
    'talk_root%': '<(webrtc_root)/../talk',
  },
  'targets': [
    {
      'target_name': 'libjingle_xmpphelp',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/third_party/expat/expat.gyp:expat',
        '<(talk_root)/libjingle.gyp:libjingle',
        '<(talk_root)/libjingle.gyp:libjingle_p2p',
      ],
      'sources': [
        'xmpp/jingleinfotask.cc',
        'xmpp/jingleinfotask.h',
      ],
    },  # target libjingle_xmpphelp
    {
      'target_name': 'jingle_session',
      'type': 'static_library',
      'dependencies': [
        '<(talk_root)/libjingle.gyp:libjingle_media',
        '<(webrtc_root)/base/base.gyp:rtc_base',
        '<(webrtc_root)/common.gyp:webrtc_common',
        '<(webrtc_root)/libjingle/xmpp/xmpp.gyp:rtc_xmpp',
        '<(DEPTH)/third_party/expat/expat.gyp:expat',
      ],
      'cflags_cc!': [
        '-Wnon-virtual-dtor',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/third_party/expat/expat.gyp:expat',
      ],
      'sources': [
        'session/constants.cc',
        'session/constants.h',
        'session/p2ptransportparser.cc',
        'session/p2ptransportparser.h',
        'session/parsing.cc',
        'session/parsing.h',
        'session/rawtransportparser.cc',
        'session/rawtransportparser.h',
        'session/sessionclient.h',
        'session/sessionmanager.h',
        'session/sessionmanager.cc',
        'session/sessionmanagertask.h',
        'session/sessionmessages.cc',
        'session/sessionmessages.h',
        'session/sessionsendtask.h',
        'session/transportparser.cc',
        'session/transportparser.h',
        'session/media/call.cc',
        'session/media/call.h',
        'session/media/mediasessionclient.cc',
        'session/media/mediasessionclient.h',
        'session/media/mediamessages.cc',
        'session/media/mediamessages.h',
        'session/tunnel/pseudotcpchannel.cc',
        'session/tunnel/pseudotcpchannel.h',
        'session/tunnel/tunnelsessionclient.cc',
        'session/tunnel/tunnelsessionclient.h',
        'session/tunnel/securetunnelsessionclient.cc',
        'session/tunnel/securetunnelsessionclient.h',
      ],
      'direct_dependent_settings': {
        'cflags_cc!': [
          '-Wnon-virtual-dtor',
        ],
        'defines': [
          'FEATURE_ENABLE_VOICEMAIL',
        ],
      },
      'conditions': [
        ['build_with_chromium==0', {
          'defines': [
            'FEATURE_ENABLE_VOICEMAIL',
            'FEATURE_ENABLE_PSTN',
          ],
        }],
      ],
    },
    {
      'target_name': 'jingle_session_unittest',
      'type': 'executable',
      'dependencies': [
        'jingle_session',
        '<(webrtc_root)/base/base_tests.gyp:rtc_base_tests_utils',
        '<(talk_root)/libjingle.gyp:libjingle',
        '<(talk_root)/libjingle.gyp:libjingle_p2p',
        '<(talk_root)/libjingle_tests.gyp:libjingle_unittest_main',
      ],
      'sources': [
        'session/media/mediamessages_unittest.cc',
        'session/media/mediasessionclient_unittest.cc',
        'session/session_unittest.cc',
        'session/transportparser_unittest.cc',
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
    },  # target jingle_session_unittest
    {
      'target_name': 'login',
      'type': 'executable',
      'dependencies': [
        'libjingle_xmpphelp',
        '<(talk_root)/libjingle.gyp:libjingle',
      ],
      'sources': [
        'examples/login/login_main.cc',
      ],
    },  # target login
  ],
  'conditions': [
    # TODO(ronghuawu): Reenable building call.
    # ['OS!="android"', {
    #   'targets': [
    #     {
    #       'target_name': 'call',
    #       'type': 'executable',
    #       'dependencies': [
    #         'libjingle.gyp:libjingle_p2p',
    #         'libjingle_xmpphelp',
    #       ],
    #       'sources': [
    #         'examples/call/call_main.cc',
    #         'examples/call/callclient.cc',
    #         'examples/call/callclient.h',
    #         'examples/call/console.cc',
    #         'examples/call/console.h',
    #         'examples/call/friendinvitesendtask.cc',
    #         'examples/call/friendinvitesendtask.h',
    #         'examples/call/mediaenginefactory.cc',
    #         'examples/call/mediaenginefactory.h',
    #         'examples/call/muc.h',
    #         'examples/call/mucinviterecvtask.cc',
    #         'examples/call/mucinviterecvtask.h',
    #         'examples/call/mucinvitesendtask.cc',
    #         'examples/call/mucinvitesendtask.h',
    #         'examples/call/presencepushtask.cc',
    #         'examples/call/presencepushtask.h',
    #       ],
    #       'conditions': [
    #         ['OS=="linux"', {
    #           'link_settings': {
    #             'libraries': [
    #               '<!@(pkg-config --libs-only-l gobject-2.0 gthread-2.0'
    #                   ' gtk+-2.0)',
    #             ],
    #           },
    #         }],
    #         ['OS=="win"', {
    #           'msvs_settings': {
    #             'VCLinkerTool': {
    #               'AdditionalDependencies': [
    #                 'strmiids.lib',
    #               ],
    #             },
    #           },
    #         }],
    #       ],  # conditions
    #     },  # target call
    #   ], # targets
    # }],  # OS!="android"
  ],
}
