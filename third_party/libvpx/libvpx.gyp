# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is based upon the libvpx.gyp file in the Chromium source code,
# but with the following targets removed: simple_encoder, simple_decoder,
# libvpx_include, libvpx_lib.
# http://src.chromium.org/svn/trunk/deps/third_party/libvpx/libvpx.gyp
{
  # The target_defaults block is unique to the WebRTC libvpx.gyp.
  'target_defaults': {
    'conditions': [
      ['clang == 1', {
        'xcode_settings': {
          'WARNING_CFLAGS': [
            # libvpx heavily relies on implicit enum casting.
            '-Wno-conversion',
            # libvpx does `if ((a == b))` in some places.
            '-Wno-parentheses-equality',
          ],
        },
        'cflags': [
          '-Wno-conversion',
          '-Wno-parentheses-equality',
        ],
      }],
    ],
  },
  'variables': {
    'conditions': [
      ['os_posix==1', {
        'asm_obj_extension': 'o',
      }],
      ['OS=="win"', {
        'asm_obj_extension': 'obj',
      }],
    ],
  },
  'conditions': [
    # TODO(andrew): Hack to ensure we pass -msse2 to gcc on Linux for files
    # containing SSE intrinsics. This should be handled in the gyp generator
    # scripts somehow. Clang (default on Mac) doesn't require this.
    ['target_arch=="ia32"', {
      'targets' : [
        {
          'target_name': 'libvpx_sse2',
          'type': 'static_library',
          'include_dirs': [
            'source/config/<(OS)/<(target_arch)',
            'source/libvpx',
            'source/libvpx/vp8/common',
            'source/libvpx/vp8/decoder',
            'source/libvpx/vp8/encoder',
          ],
          'sources': [
            'source/libvpx/vp8/encoder/x86/denoising_sse2.c',
          ],
          'cflags': [ '-msse2', ],
        },
      ],
    }],
    [ '(OS=="linux" or OS=="mac" or OS=="win") and target_arch!="arm"', {
      'targets': [
        {
          # This libvpx target contains both encoder and decoder.
          # Encoder is configured to be realtime only.
          'target_name': 'libvpx',
          'type': 'static_library',
          'variables': {
            'yasm_output_path': '<(SHARED_INTERMEDIATE_DIR)/third_party/libvpx',
            'yasm_flags': [
              '-I', 'source/config/<(OS)/<(target_arch)',
              '-I', 'source/libvpx',
            ],
          },
          'includes': [
            '../yasm/yasm_compile.gypi'
          ],
          'include_dirs': [
            'source/config/<(OS)/<(target_arch)',
            'source/libvpx',
            'source/libvpx/vp8/common',
            'source/libvpx/vp8/decoder',
            'source/libvpx/vp8/encoder',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'source/libvpx',
            ],
          },
          'conditions': [
            [ 'target_arch=="ia32"', {
              'includes': [
                'libvpx_srcs_x86.gypi',
              ],
              'dependencies': [ 'libvpx_sse2', ],
            }],
            [ 'target_arch=="x64"', {
              'includes': [
                'libvpx_srcs_x86_64.gypi',
              ],
            }],
          ],
        },
      ],
    },
    ],
    # 'libvpx' target for Chrome OS ARM builds.
    [ 'target_arch=="arm" ', {
      'targets': [
        {
          # This libvpx target contains both encoder and decoder.
          # Encoder is configured to be realtime only.
          'target_name': 'libvpx',
          'type': 'static_library',

          # Copy the script to the output folder so that we can use it with
          # absolute path.
          'copies': [{
            'destination': '<(shared_generated_dir)',
            'files': [
              '<(ads2gas_script_path)',
            ],
          }],

          # Rule to convert .asm files to .S files.
          'rules': [
            {
              'rule_name': 'convert_asm',
              'extension': 'asm',
              'inputs': [ '<(shared_generated_dir)/<(ads2gas_script)', ],
              'outputs': [
                '<(shared_generated_dir)/<(RULE_INPUT_ROOT).S',
              ],
              'action': [
                'bash',
                '-c',
                'cat <(RULE_INPUT_PATH) | perl <(shared_generated_dir)/<(ads2gas_script) > <(shared_generated_dir)/<(RULE_INPUT_ROOT).S',
              ],
              'process_outputs_as_sources': 1,
              'message': 'Convert libvpx asm file for ARM <(RULE_INPUT_PATH).',
            },
          ],

          'variables': {
            # Location of the assembly conversion script.
            'ads2gas_script': 'ads2gas.pl',
            'ads2gas_script_path': 'source/libvpx/build/make/<(ads2gas_script)',

            # Location of the intermediate output.
            'shared_generated_dir': '<(SHARED_INTERMEDIATE_DIR)/third_party/libvpx',

            # Conditions to generate arm-neon as an target.
            'conditions': [
              ['target_arch=="arm" and arm_neon==1', {
                'target_arch_full': 'arm-neon',
              }, {
                'target_arch_full': '<(target_arch)',
              }],
              ['OS=="android"', {
                '_OS': 'linux',
              }, {
                '_OS': '<(OS)',
              }],
            ],
          },
          'cflags': [
            # We need to explicitly tell the GCC assembler to look for
            # .include directive files from the place where they're
            # generated to.
            '-Wa,-I,third_party/libvpx/source/config/<(_OS)/<(target_arch_full)',
          ],
          'include_dirs': [
            'source/config/<(_OS)/<(target_arch_full)',
            'source/libvpx',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'source/libvpx',
            ],
          },
          'conditions': [
            # Libvpx optimizations for ARMv6 or ARMv7 without NEON.
            ['arm_neon==0', {
              'includes': [
                'libvpx_srcs_arm.gypi',
              ],
            }],
            # Libvpx optimizations for ARMv7 with NEON.
            ['arm_neon==1', {
              'includes': [
                'libvpx_srcs_arm_neon.gypi',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
