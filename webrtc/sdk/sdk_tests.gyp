# Copyright 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [ '../build/common.gypi', ],
  'conditions': [
    ['OS=="ios" or (OS=="mac" and mac_deployment_target=="10.7")', {
      'targets': [
        {
          'target_name': 'rtc_sdk_peerconnection_objc_tests',
          'type': 'executable',
          'includes': [
            '../build/objc_common.gypi',
          ],
          'dependencies': [
            '<(webrtc_root)/base/base_tests.gyp:rtc_base_tests_utils',
            '<(webrtc_root)/sdk/sdk.gyp:rtc_sdk_peerconnection_objc',
          ],
          'sources': [
            'objc/Framework/UnitTests/RTCConfigurationTest.mm',
            'objc/Framework/UnitTests/RTCDataChannelConfigurationTest.mm',
            'objc/Framework/UnitTests/RTCIceCandidateTest.mm',
            'objc/Framework/UnitTests/RTCIceServerTest.mm',
            'objc/Framework/UnitTests/RTCMediaConstraintsTest.mm',
            'objc/Framework/UnitTests/RTCSessionDescriptionTest.mm',
          ],
          'xcode_settings': {
            # |-ObjC| flag needed to make sure category method implementations
            # are included:
            # https://developer.apple.com/library/mac/qa/qa1490/_index.html
            'OTHER_LDFLAGS': ['-ObjC'],
          },
        },
      ],
    }],  # OS=="ios" or (OS=="mac" and mac_deployment_target=="10.7")
  ],
}
