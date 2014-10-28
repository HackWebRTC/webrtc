# Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [ '../../build/common.gypi', ],
  'targets': [
    {
      'target_name': 'rtc_xmpp_unittest',
      'type': 'none',
      'direct_dependent_settings': {
        'sources': [
          'fakexmppclient.h',
          'hangoutpubsubclient_unittest.cc',
          'jid_unittest.cc',
          'mucroomconfigtask_unittest.cc',
          'mucroomdiscoverytask_unittest.cc',
          'mucroomlookuptask_unittest.cc',
          'mucroomuniquehangoutidtask_unittest.cc',
          'pingtask_unittest.cc',
          'pubsubclient_unittest.cc',
          'pubsubtasks_unittest.cc',
          'util_unittest.cc',
          'util_unittest.h',
          'xmppengine_unittest.cc',
          'xmpplogintask_unittest.cc',
          'xmppstanzaparser_unittest.cc',
        ],
      },
    },
  ],
}
  
