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
        '../../../../../../common_video/interface',
        '../../../../../../common_video/vplib/main/interface',
        '../../../interface',
        '../../../../../interface',
      ],
      'conditions': [
        ['build_with_chromium==1', {
           'conditions': [
             ['target_arch=="arm"', {
               'dependencies': [
                 '<(webrtc_root)/../libvpx/libvpx.gyp:libvpx_lib',
                 '<(webrtc_root)/../libvpx/libvpx.gyp:libvpx_include',
               ],
             }, {  # arm
               'conditions': [
                 ['OS=="win"', {
                   'dependencies': [
                     # We don't want to link with the static library inside Chromium
                     # on Windows. Chromium uses the ffmpeg DLL and exports the
                     # necessary libvpx symbols for us.
                     '<(webrtc_root)/../libvpx/libvpx.gyp:libvpx_include',
                   ],
                 },{ # non-arm, win
                   'dependencies': [
                     '<(webrtc_root)/../libvpx/libvpx.gyp:libvpx',
                   ],
                   'include_dirs': [
                     '../../../../../../../libvpx/source/libvpx',
                   ],
                 }], # non-arm, non-win
               ],
             }],
           ],
           'defines': [
             'WEBRTC_LIBVPX_VERSION=960' # Bali
           ],
        },{
          'dependencies': [
            '<(webrtc_root)/../third_party/libvpx/libvpx.gyp:libvpx',
          ],
          'include_dirs': [
            '../../../../../../../third_party/libvpx/source/libvpx',
          ],
          'defines': [
            'WEBRTC_LIBVPX_VERSION=971' # Cayuga
          ],
        }],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
          '../../../../../../common_video/interface',
          '../../../interface',
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
