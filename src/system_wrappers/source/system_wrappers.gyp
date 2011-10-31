# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# TODO: Rename files to use *_linux.cpp etc. names, to automatically include relevant files. Remove conditions section.

{
  'includes': [
    '../../common_settings.gypi', # Common settings
  ],
  'targets': [
    {
      'target_name': 'system_wrappers',
      'type': '<(library)',
      'include_dirs': [
        'spreadsortlib',
        '../interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
        ],
      },
      'sources': [
        '../interface/aligned_malloc.h',
        '../interface/atomic32_wrapper.h',
        '../interface/condition_variable_wrapper.h',
        '../interface/cpu_wrapper.h',
        '../interface/cpu_features_wrapper.h',
        '../interface/critical_section_wrapper.h',
        '../interface/data_log.h',
        '../interface/data_log_c.h',
        '../interface/data_log_impl.h',
        '../interface/event_wrapper.h',
        '../interface/file_wrapper.h',
        '../interface/fix_interlocked_exchange_pointer_windows.h',
        '../interface/list_wrapper.h',
        '../interface/map_wrapper.h',
        '../interface/ref_count.h',
        '../interface/rw_lock_wrapper.h',
        '../interface/scoped_ptr.h',
        '../interface/scoped_refptr.h',
        '../interface/sort.h',
        '../interface/thread_wrapper.h',
        '../interface/tick_util.h',
        '../interface/trace.h',
        'aligned_malloc.cc',
        'atomic32.cc',
        'atomic32_linux.h',
        'atomic32_mac.h',
        'atomic32_windows.h',
        'condition_variable.cc',
        'condition_variable_posix.h',
        'condition_variable_windows.h',
        'cpu.cc',
        'cpu_linux.h',
        'cpu_mac.h',
        'cpu_windows.h',
        'cpu_features.cc',
        'critical_section.cc',
        'critical_section_posix.h',
        'critical_section_windows.h',
        'data_log_c.cc',
        'event.cc',
        'event_posix.h',
        'event_windows.h',
        'file_impl.cc',
        'file_impl.h',
        'list_no_stl.cc',
        'map.cc',
        'rw_lock.cc',
        'rw_lock_posix.h',
        'rw_lock_windows.h',
        'sort.cc',
        'thread.cc',
        'thread_posix.h',
        'thread_windows.h',
        'thread_windows_set_name.h',
        'trace_impl.cc',
        'trace_impl.h',
        'trace_posix.h',
        'trace_windows.h',
      ],
      'conditions': [
        ['os_posix==1', {
          'sources': [
            'condition_variable_posix.cc',
            'critical_section_posix.cc',
            'event_posix.cc',
            'rw_lock_posix.cc',
            'thread_posix.cc',
            'trace_posix.cc',
          ],
        }],
        ['enable_data_logging==1', {
          'sources': [
            'data_log.cc',
          ],
        },{
          'sources': [
            'data_log_dummy.cc',
          ],
        },],
        ['OS=="linux"', {
          'sources': [
            'cpu_linux.cc',
          ],
          'link_settings': {
            'libraries': [
              '-lrt',
            ],
          },
        }],
        ['OS=="mac"', {
          'sources': [
            'cpu_mac.cc',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/ApplicationServices.framework',
            ],
          },
        }],
        ['OS=="win"', {
          'sources': [
            'condition_variable_windows.cc',
            'cpu_windows.cc',
            'critical_section_windows.cc',
            'event_windows.cc',
            'rw_lock_windows.cc',
            'thread_windows.cc',
            'trace_windows.cc',
          ],
          'link_settings': {
            'libraries': [
              '-lwinmm.lib',
            ],
          },
        }],
      ] # conditions
    },
  ], # targets
  'conditions': [
    ['build_with_chromium==0', {
      'targets': [
        {
          'target_name': 'system_wrappers_unittests',
          'type': 'executable',
          'dependencies': [
            'system_wrappers',
            '<(webrtc_root)/../testing/gtest.gyp:gtest',
            '<(webrtc_root)/../test/test.gyp:test_support',
          ],
          'sources': [
            'cpu_wrapper_unittest.cc',
            'list_unittest.cc',
            'map_unittest.cc',
            'data_log_helpers_unittest.cc',
            'data_log_c_helpers_unittest.c',
            'data_log_c_helpers_unittest.h',
            '<(webrtc_root)/../test/run_all_unittests.cc',
          ],
          'conditions': [
            ['enable_data_logging==1', {
              'sources': [ 'data_log_unittest.cc', ],
            }, {
              'sources': [ 'data_log_unittest_disabled.cc', ],
            }],
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
