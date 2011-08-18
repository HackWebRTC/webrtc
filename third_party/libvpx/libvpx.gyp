# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'libvpx',
      'type': 'static_library',
      'variables': {
        'shared_generated_dir':
          '<(SHARED_INTERMEDIATE_DIR)/third_party/libvpx',
        'yasm_path': '<(PRODUCT_DIR)/yasm',
        'yasm_flags': [
          '-I', 'source/config/<(OS)/<(target_arch)',
          '-I', 'source/libvpx'
        ],
      },
      'conditions': [
        ['OS=="linux"', {
          'variables': {
            'asm_obj_dir':
              '<(shared_generated_dir)',
            'obj_file_ending':
              'o',
            'conditions': [
              ['target_arch=="ia32"', {
                'yasm_flags': [
                  '-felf32',
                  '-m', 'x86',
                ],
              },],
              ['target_arch=="x64"', {
                'yasm_flags': [
                  '-felf64',
                  '-m', 'amd64',
                ],
              },],
            ],
          },
          'dependencies': [
            '../yasm/yasm.gyp:yasm#host',
          ],
          'includes': [
            'input_files_linux.gypi',
          ],
        },],
        ['OS=="mac"', {
          'variables': {
            'asm_obj_dir':
              '<(shared_generated_dir)',
            'obj_file_ending':
              'o',
            'conditions': [
              ['target_arch=="ia32"', {
                'yasm_flags': [
                  '-fmacho32',
                  '-m', 'x86',
                ],
              },],
              ['target_arch=="x64"', {
                'yasm_flags': [
                  '-fmacho64',
                  '-m', 'amd64',
                ],
              },],
            ],
          },
          'dependencies': [
            '../yasm/yasm.gyp:yasm#host',
          ],
          'includes': [
            'input_files_mac.gypi',
          ],
        },],
        ['OS=="win"', {
          # Don't build yasm from source on Windows
          'variables': {
            'asm_obj_dir':
              'asm',
            'obj_file_ending':
              'obj',
            'yasm_path': '../yasm/binaries/win/yasm.exe',
            'conditions': [
              ['target_arch=="ia32"', {
                'yasm_flags': [
                  '-fwin32',
                  '-m', 'x86',
                ],
              },],
              ['target_arch=="x64"', {
                'yasm_flags': [
                  '-fwin64',
                  '-m', 'amd64',
                ],
              },],
            ],
          },
          'includes': [
            'input_files_win.gypi',
          ],
        },],
      ],
      
      'include_dirs': [
        'source/config/<(OS)/<(target_arch)',
        'source/libvpx/build',
        'source/libvpx/',
        'source/libvpx/vp8/common',
        'source/libvpx/vp8/decoder',
        'source/libvpx/vp8/encoder',
      ],
      
      'rules': [
        {
          'rule_name': 'assemble',
          'extension': 'asm',
          'inputs': [ '<(yasm_path)', ],
          'outputs': [
            '<(asm_obj_dir)/<(RULE_INPUT_ROOT).<(obj_file_ending)',
          ],
          'action': [
            '<(yasm_path)',
            '<@(yasm_flags)',
            '-o', '<(asm_obj_dir)/<(RULE_INPUT_ROOT).<(obj_file_ending)',
            '<(RULE_INPUT_PATH)',
          ],
          'process_outputs_as_sources': 1,
          'message': 'Build libvpx yasm build <(RULE_INPUT_PATH).',
        },
      ],
    }
  ]
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
