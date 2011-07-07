# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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
        '../interface/event_wrapper.h',
        '../interface/file_wrapper.h',
        '../interface/list_wrapper.h',
        '../interface/map_wrapper.h',
        '../interface/rw_lock_wrapper.h',
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
        'condition_variable_linux.h',
        'condition_variable_windows.h',
        'cpu.cc',
        'cpu_linux.h',
        'cpu_mac.h',
        'cpu_windows.h',
        'cpu_features.cc',
        'critical_section.cc',
        'critical_section_linux.h',
        'critical_section_windows.h',
        'event.cc',
        'event_linux.h',
        'event_windows.h',
        'file_impl.cc',
        'file_impl.h',
        'list_no_stl.cc',
        'map.cc',
        'rw_lock.cc',
        'rw_lock_linux.h',
        'rw_lock_windows.h',
        'sort.cc',
        'thread.cc',
        'thread_linux.h',
        'thread_windows.h',
        'trace_impl.cc',
        'trace_impl.h',
        'trace_linux.h',
        'trace_windows.h',
      ],
      'conditions': [
        ['OS=="linux"', {
          'sources': [
            'condition_variable_linux.cc',
            'cpu_linux.cc',
            'critical_section_linux.cc',
            'event_linux.cc',
            'thread_linux.cc',
            'trace_linux.cc',
            'rw_lock_linux.cc',
          ],
          'link_settings': {
            'libraries': [
              '-lrt',
            ],
          },
        }],
        ['OS=="mac"', {
          'sources': [
            'condition_variable_linux.cc',
            'cpu_mac.cc',
            'critical_section_linux.cc',
            'event_linux.cc',
            'rw_lock_linux.cc',
            'thread_linux.cc',
            'trace_linux.cc',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            'atomic32_windows.h',
            'condition_variable_windows.cc',
            'condition_variable_windows.h',
            'cpu_windows.cc',
            'cpu_windows.h',
            'critical_section_windows.cc',
            'critical_section_windows.h',
            'event_windows.cc',
            'event_windows.h',
            'rw_lock_windows.cc',
            'rw_lock_windows.h',
            'thread_windows.cc',
            'thread_windows.h',
            'trace_windows.cc',
            'trace_windows.h',
          ],
          'link_settings': {
            'libraries': [
              '-lwinmm.lib',
            ],
          },
        }],
	    ] # conditions
    },
    {
      'target_name': 'system_wrappersTest',
      'type': 'executable',
      'dependencies': [
        'system_wrappers'
      ],
      'sources': [
        '../test/Test.cpp',
      ],
    },
  ], # targets
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
