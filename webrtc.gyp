# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    'src/common_settings.gypi', # Common settings
  ],
 'targets': [
    {
      'target_name': 'auto_tests',
      'type': 'none',
      'dependencies': [
        'src/voice_engine.gyp:voe_auto_test',
        'src/video_engine.gyp:vie_auto_test',
      ],
    },
    {
      'target_name': 'cmd_test',
      'type': 'none',
      'dependencies': [
        'src/voice_engine.gyp:voe_cmd_test',
      ],
    },    
    {
      'target_name': 'peerconnection_server',
      'type': 'executable',
      'sources': [
        'peerconnection/samples/server/data_socket.cc',
        'peerconnection/samples/server/data_socket.h',
        'peerconnection/samples/server/main.cc',
        'peerconnection/samples/server/peer_channel.cc',
        'peerconnection/samples/server/peer_channel.h',
        'peerconnection/samples/server/utils.cc',
        'peerconnection/samples/server/utils.h',
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'peerconnection_client',
          'type': 'executable',
          'sources': [
            'peerconnection/samples/client/conductor.cc',
            'peerconnection/samples/client/conductor.h',
            'peerconnection/samples/client/defaults.cc',
            'peerconnection/samples/client/defaults.h',
            'peerconnection/samples/client/main.cc',
            'peerconnection/samples/client/main_wnd.cc',
            'peerconnection/samples/client/main_wnd.h',
            'peerconnection/samples/client/peer_connection_client.cc',
            'peerconnection/samples/client/peer_connection_client.h',
            'third_party/libjingle/source/talk/base/win32socketinit.cc',
            'third_party/libjingle/source/talk/base/win32socketserver.cc',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
             'SubSystem': '2',   # Windows
            },
          },
          'dependencies': [
            'third_party_mods/libjingle/libjingle.gyp:libjingle_app',
          ],
          'include_dirs': [
            'third_party/libjingle/source',
            'third_party_mods/libjingle/source',
          ],
        },
      ],  # targets
    }, ],  # OS="win"
    ['OS=="linux"', {
      'targets': [
        {
          'target_name': 'peerconnection_client',
          'type': 'executable',
          'sources': [
            'peerconnection/samples/client/conductor.cc',
            'peerconnection/samples/client/conductor.h',
            'peerconnection/samples/client/defaults.cc',
            'peerconnection/samples/client/defaults.h',
            'peerconnection/samples/client/linux/main.cc',
            'peerconnection/samples/client/linux/main_wnd.cc',
            'peerconnection/samples/client/linux/main_wnd.h',
            'peerconnection/samples/client/peer_connection_client.cc',
            'peerconnection/samples/client/peer_connection_client.h',
          ],
          'dependencies': [
            'third_party_mods/libjingle/libjingle.gyp:libjingle_app',
            # TODO(tommi): Switch to this and remove specific gtk dependency
            # sections below for cflags and link_settings.
            # '<(DEPTH)/build/linux/system.gyp:gtk',
          ],
          'include_dirs': [
            'third_party/libjingle/source',
            'third_party_mods/libjingle/source',
          ],
          'cflags': [
            '<!@(pkg-config --cflags gtk+-2.0)',
          ],
          'link_settings': {
            'ldflags': [
              '<!@(pkg-config --libs-only-L --libs-only-other gtk+-2.0 gthread-2.0)',
            ],
            'libraries': [
              '<!@(pkg-config --libs-only-l gtk+-2.0 gthread-2.0)',
              '-lX11',
              '-lXext',
            ],
          },
        },
      ],  # targets
    }, ],  # OS="linux"
  ],  # conditions  
}
