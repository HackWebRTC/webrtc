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
      'target_name': 'rtc_xmllite_unittest',
      'type': 'executable',
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(webrtc_root)/base/base_tests.gyp:rtc_base_tests_utils',
        'xmllite.gyp:rtc_xmllite',
      ],
      'cflags_cc!': [
        '-Wnon-virtual-dtor',
      ],
      'sources': [
        'qname_unittest.cc',
        'xmlbuilder_unittest.cc',
        'xmlelement_unittest.cc',
        'xmlnsstack_unittest.cc',
        'xmlparser_unittest.cc',
        'xmlprinter_unittest.cc',
      ],
    },
  ],
}
  
