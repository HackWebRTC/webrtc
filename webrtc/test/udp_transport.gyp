# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'udp_transport',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'udp_transport/include',
        ],
      },
      'sources': [
        # PLATFORM INDEPENDENT SOURCE FILES
        'udp_transport/channel_transport.cc',
        'udp_transport/include/channel_transport.h',
        'udp_transport/udp_transport.h',
        'udp_transport/udp_transport_impl.cc',
        'udp_transport/udp_socket_wrapper.cc',
        'udp_transport/udp_socket_manager_wrapper.cc',
        'udp_transport/udp_transport_impl.h',
        'udp_transport/udp_socket_wrapper.h',
        'udp_transport/udp_socket_manager_wrapper.h',
        # PLATFORM SPECIFIC SOURCE FILES - Will be filtered below
        # Posix (Linux/Mac)
        'udp_transport/udp_socket_posix.cc',
        'udp_transport/udp_socket_posix.h',
        'udp_transport/udp_socket_manager_posix.cc',
        'udp_transport/udp_socket_manager_posix.h',
        # win
        'udp_transport/udp_socket2_manager_win.cc',
        'udp_transport/udp_socket2_manager_win.h',
        'udp_transport/udp_socket2_win.cc',
        'udp_transport/udp_socket2_win.h',
        'udp_transport/traffic_control_win.cc',
        'udp_transport/traffic_control_win.h',
      ], # source
    },
    {
      'target_name': 'udp_transport_unittests',
      'type': 'executable',
      'dependencies': [
        'udp_transport',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(webrtc_root)/test/test.gyp:test_support_main',
      ],
      'sources': [
        'udp_transport/udp_transport_unittest.cc',
        'udp_transport/udp_socket_manager_unittest.cc',
        'udp_transport/udp_socket_wrapper_unittest.cc',
      ],
      # Disable warnings to enable Win64 build, issue 1323.
      'msvs_disabled_warnings': [
        4267,  # size_t to int truncation.
      ],
    }, # udp_transport_unittests
  ], # targets
}
