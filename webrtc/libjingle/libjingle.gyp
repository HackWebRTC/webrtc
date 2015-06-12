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
        '<(talk_root)/libjingle.gyp:libjingle',
        '<(talk_root)/libjingle.gyp:libjingle_p2p',
      ],
      'conditions': [
        ['build_expat==1', {
          'dependencies': [
            '<(DEPTH)/third_party/expat/expat.gyp:expat',
          ],
        }],
      ],
      'sources': [
        'xmpp/jingleinfotask.cc',
        'xmpp/jingleinfotask.h',
      ],
    },  # target libjingle_xmpphelp
  ],
}
