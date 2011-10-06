# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  # Exclude the test target when building with chromium.
  'conditions': [   
    ['build_with_chromium==0', {
      'targets': [
        {
          'target_name': 'video_codecs_test_framework',
          'type': '<(library)',
          'dependencies': [
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/common_video/common_video.gyp:webrtc_vplib',
            '<(webrtc_root)/../testing/gtest.gyp:gtest',         
            '<(webrtc_root)/../third_party/google-gflags/google-gflags.gyp:google-gflags',
          ],
          'include_dirs': [
            '../interface',
            '<(webrtc_root)/common_video/interface',
            '<(webrtc_root)/../testing/gtest/include',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '../interface',
              '<(webrtc_root)/../testing/gtest/include',
            ],
          },
          'sources': [
            # header files
            'file_handler.h',
            'packet_manipulator.h',
            'packet_reader.h',
            'stats.h',
            'videoprocessor.h',
            'util.h',

            # source files
            'file_handler.cc',
            'packet_manipulator.cc',
            'packet_reader.cc',
            'stats.cc',
            'videoprocessor.cc',
            'util.cc',            
          ],
        },
        {
          'target_name': 'video_codecs_test_framework_unittests',
          'type': 'executable',
          'dependencies': [
            'video_codecs_test_framework',            
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/common_video/common_video.gyp:webrtc_vplib',
            '<(webrtc_root)/../testing/gmock.gyp:gmock',    
            '<(webrtc_root)/../test/test.gyp:test_support',
          ],
          'include_dirs': [
             '<(webrtc_root)/common_video/interface',               
           ],
           'sources': [
            # header files
            'mocks.h',
            
            # source files
            'file_handler_unittest.cc',
            'packet_manipulator_unittest.cc',
            'packet_reader_unittest.cc',
            # cannot use the global run all file until it supports gmock:
            'run_all_unittests.cc',
            'stats_unittest.cc',
            'videoprocessor_unittest.cc',
          ],
        },
      ], # targets
    }], # build_with_chromium
  ], # conditions
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
