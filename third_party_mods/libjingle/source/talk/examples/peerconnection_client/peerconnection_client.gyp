# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'conditions': [
    ['OS=="linux"', {
      'targets': [
        {
          'target_name': 'peerconnection_client_dev',
          'type': 'executable',
          'sources': [
            'conductor.cc',
            'conductor.h',
            'defaults.cc',
            'defaults.h',
            'linux/main.cc',
            'linux/main_wnd.cc',
            'linux/main_wnd.h',
            'peer_connection_client.cc',
            'peer_connection_client.h',
          ],
          'dependencies': [
            '../../../../libjingle.gyp:libjingle_app',
            '../../../../../../src/modules/modules.gyp:video_capture_module',
            '../../../../../../src/system_wrappers/source/'
                'system_wrappers.gyp:system_wrappers',
            # TODO(tommi): Switch to this and remove specific gtk dependency
            # sections below for cflags and link_settings.
            # '<(DEPTH)/build/linux/system.gyp:gtk',
          ],
          'include_dirs': [           
            '../../../',
            '../../../../../../src', # webrtc modules
            #TODO(perkj): Remove when this project is in the correct folder.
             '../../../../../../third_party/libjingle/source/',
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