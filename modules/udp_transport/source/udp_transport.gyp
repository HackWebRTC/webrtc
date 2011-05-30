# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../common_settings.gypi', # Common settings
  ],
  'targets': [
    {
      'target_name': 'udp_transport',
      'type': '<(library)',
      'dependencies': [
        '../../../system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        '../interface',
        '../../interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
          '../../interface',
        ],
      },
      'sources': [
        # PLATFORM INDEPENDENT SOURCE FILES
        '../interface/udp_transport.h',
        'udp_transport_impl.cc',
        'udp_socket_wrapper.cc',
        'udp_socket_manager_wrapper.cc',
        'udp_transport_impl.h',
        'udp_socket_wrapper.h',
        'udp_socket_manager_wrapper.h',
        # PLATFORM SPECIFIC SOURCE FILES - Will be filtered below
        # Linux/Mac
        'udp_socket_linux.cc',
        'udp_socket_linux.h',
        'udp_socket_manager_linux.cc',
        'udp_socket_manager_linux.h',
        # Windows
        'udp_socket_manager_windows.cc',
        'udp_socket_manager_windows.h',
        'udp_socket2_manager_windows.cc',
        'udp_socket2_manager_windows.h',
        'udp_socket_windows.cc',
        'udp_socket_windows.h',
        'udp_socket2_windows.cc',
        'udp_socket2_windows.h',
        'traffic_control_windows.cc',
        'traffic_control_windows.h',
        # WinCE
        # FIXME don't think we need CE any more
        #'udp_socket_manager_windows_ce.cc',
        #'udp_socket2_manager_windows_ce.cc',
        #'udp_socket2_windows_ce.cc',
      ], # source
      'conditions': [
        # DEFINE PLATFORM SPECIFIC SOURCE FILES
        ['OS!="linux" and OS!="mac"', {
          'sources!': [
            'udp_socket_linux.cc',
            'udp_socket_linux.h',
            'udp_socket_manager_linux.cc',
            'udp_socket_manager_linux.h',
          ],
        }],
        ['OS!="win"', {
          'sources!': [
            'udp_socket_manager_windows.cc',
            'udp_socket_manager_windows.h',
            'udp_socket2_manager_windows.cc',
            'udp_socket2_manager_windows.h',
            'udp_socket_windows.cc',
            'udp_socket_windows.h',
            'udp_socket2_windows.cc',
            'udp_socket2_windows.h',
            'traffic_control_windows.cc',
            'traffic_control_windows.h',
          ],
        }],
        ['OS=="linux"', {
          'cflags': [
            '-fno-strict-aliasing',
          ],
        }],        
        ['OS=="mac"', {
          'xcode_settings': {
            'OTHER_CPLUSPLUSFLAGS': '-fno-strict-aliasing',
          },
        }],
        ['OS=="win"', {
          'defines': [
            'USE_WINSOCK2',
          ],
        }],
      ] # conditions
    },
  ], # targets
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
