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
        '<(webrtc_root)/base/base.gyp:webrtc_base',
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
