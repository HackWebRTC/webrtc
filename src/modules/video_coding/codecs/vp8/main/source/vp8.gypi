# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'webrtc_vp8',
      'type': '<(library)',
      'dependencies': [
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        '../interface',
        '<(webrtc_root)/common_video/interface',
        '<(webrtc_root)/common_video/vplib/main/interface',
        '<(webrtc_root)/modules/video_coding/codecs/interface',
        '<(webrtc_root)/modules/interface',
      ],
      'conditions': [
        ['build_with_chromium==1', {
          'dependencies': [
            '<(webrtc_root)/../libvpx/libvpx.gyp:libvpx',
          ],
          'defines': [
            'WEBRTC_LIBVPX_VERSION=960' # Bali
          ],
        },{
          'dependencies': [
            '<(webrtc_root)/../third_party/libvpx/libvpx.gyp:libvpx',
          ],
          'defines': [
            'WEBRTC_LIBVPX_VERSION=971' # Cayuga
          ],
        }],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
          '<(webrtc_root)/common_video/interface',
          '<(webrtc_root)/modules/video_coding/codecs/interface',
        ],
      },
      'sources': [
        '../interface/vp8.h',
        '../interface/vp8_simulcast.h',
        'vp8.cc',
        'vp8_simulcast.cc',
      ],
    },
  ], # targets
  # Exclude the test target when building with chromium.
  'conditions': [   
    ['build_with_chromium==0', {
      'targets': [
        {
          'target_name': 'vp8_test',
          'type': 'executable',
          'dependencies': [
            'test_framework',
            'webrtc_vp8',
            '<(webrtc_root)/common_video/common_video.gyp:webrtc_vplib',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
          ],
         'sources': [
            # header files
            '../test/benchmark.h',
            '../test/normal_async_test.h',
            '../test/packet_loss_test.h',
            '../test/unit_test.h',
            '../test/dual_decoder_test.h',

           # source files
            '../test/benchmark.cc',
            '../test/normal_async_test.cc',
            '../test/packet_loss_test.cc',
            '../test/tester.cc',
            '../test/unit_test.cc',
            '../test/dual_decoder_test.cc',
          ],
        },
      ], # targets
    }], # build_with_chromium
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
