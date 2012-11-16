# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'libvpx_src_dir%': 'source/libvpx',
    'conditions': [
      ['os_posix==1', {
        'asm_obj_extension': 'o',
      }],
      ['OS=="win"', {
        'asm_obj_extension': 'obj',
      }],

      ['target_arch=="arm" and arm_neon==1', {
        'target_arch_full': 'arm-neon',
      }, {
        'target_arch_full': '<(target_arch)',
      }],

      # Conversion to libvpx arch names.
      ['target_arch=="arm" and arm_neon==1', {
        'libvpx_arch': 'armv7',
      }],
      ['target_arch=="arm" and arm_neon==0', {
        'libvpx_arch': 'armv6',
      }],
      ['target_arch=="ia32"', {
        'libvpx_arch': 'x86',
      }],
      ['target_arch=="x64"', {
        'libvpx_arch': 'x86_64',
      }],

      ['os_posix == 1 and OS != "mac" and OS != "android"', {
        'OS_CATEGORY%': 'linux',
      }, {
        'OS_CATEGORY%': '<(OS)',
      }],
    ],

    # Location of the intermediate output.
    'shared_generated_dir': '<(SHARED_INTERMEDIATE_DIR)/third_party/libvpx',
  },

  'conditions': [
    # TODO(andrew): Hack to ensure we pass -msse2 to gcc on Linux for files
    # containing SSE intrinsics. This should be handled in the gyp generator
    # scripts somehow. Clang (default on Mac) doesn't require this.
    ['target_arch=="ia32" or target_arch=="x64"', {
      'targets' : [
        {
          'target_name': 'libvpx_sse2',
          'type': 'static_library',
          'include_dirs': [
            'source/config/<(OS)/<(target_arch)',
            '<(libvpx_src_dir)',
            '<(libvpx_src_dir)/vp8/common',
            '<(libvpx_src_dir)/vp8/decoder',
            '<(libvpx_src_dir)/vp8/encoder',
          ],
          'sources': [
            '<(libvpx_src_dir)/vp8/encoder/x86/denoising_sse2.c',
          ],
          'conditions': [
            ['os_posix==1 and OS!="mac"', {
              'cflags': [ '-msse2', ],
            }],
            ['OS=="mac"', {
              'xcode_settings': {
                'OTHER_CFLAGS': [ '-msse2', ],
              },
            }],
          ],
        },
      ],
    }],
    [ 'target_arch!="arm"', {
      'targets': [
        {
          # This libvpx target contains both encoder and decoder.
          # Encoder is configured to be realtime only.
          'target_name': 'libvpx',
          'type': 'static_library',
          'variables': {
            'yasm_output_path': '<(SHARED_INTERMEDIATE_DIR)/third_party/libvpx',
            'OS_CATEGORY%': '<(OS_CATEGORY)',
            'yasm_flags': [
              '-D', 'CHROMIUM',
              '-I', 'source/config/<(OS_CATEGORY)/<(target_arch)',
              '-I', '<(libvpx_src_dir)',
              '-I', '<(shared_generated_dir)', # Generated assembly offsets
            ],
          },
          'dependencies': [
            'gen_asm_offsets',
          ],
          'includes': [
            '../yasm/yasm_compile.gypi'
          ],
          'include_dirs': [
            'source/config/<(OS_CATEGORY)/<(target_arch)',
            '<(libvpx_src_dir)',
            '<(libvpx_src_dir)/vp8/common',
            '<(libvpx_src_dir)/vp8/decoder',
            '<(libvpx_src_dir)/vp8/encoder',
            '<(shared_generated_dir)', # Provides vpx_rtcd.h.
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(libvpx_src_dir)',
            ],
          },
          # VS2010 does not correctly incrementally link obj files generated
          # from asm files. This flag disables UseLibraryDependencyInputs to
          # avoid this problem.
          'msvs_2010_disable_uldi_when_referenced': 1,
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
              'dependencies': [ 'libvpx_sse2', ],
            }],
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
            [ 'chromeos == 1', {
              # ChromeOS needs these files for animated WebM avatars.
              'sources': [
                '<(libvpx_src_dir)/libmkv/EbmlIDs.h',
                '<(libvpx_src_dir)/libmkv/EbmlWriter.c',
                '<(libvpx_src_dir)/libmkv/EbmlWriter.h',
              ],
            }],
          ],
        },
      ],
    },
    ],
    # 'libvpx' target for ARM builds.
    [ 'target_arch=="arm" ', {
      'targets': [
        {
          # This libvpx target contains both encoder and decoder.
          # Encoder is configured to be realtime only.
          'target_name': 'libvpx',
          'type': 'static_library',
          'dependencies': [
            'gen_asm_offsets',
          ],

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
            'ads2gas_script_path': '<(libvpx_src_dir)/build/make/<(ads2gas_script)',
          },
          'cflags': [
            # We need to explicitly tell the GCC assembler to look for
            # .include directive files from the place where they're
            # generated to.
            '-Wa,-I,<!(pwd)/source/config/<(OS_CATEGORY)/<(target_arch_full)',
            '-Wa,-I,<(shared_generated_dir)',
          ],
          'include_dirs': [
            'source/config/<(OS_CATEGORY)/<(target_arch_full)',
            '<(libvpx_src_dir)',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(libvpx_src_dir)',
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
            ['arm_neon==1 or OS == "android"', {
              'includes': [
                'libvpx_srcs_arm_neon.gypi',
              ],
              'cflags!': [
                '-mfpu=vfpv3-d16',
              ], 
              'cflags': [
                '-mfpu=neon',
              ],
            }],
            ['OS == "android"', {
              'cflags': [
                '-I<(android_ndk_root)/sources/android/cpufeatures',
              ],
            }],
            [ 'chromeos == 1', {
              # ChromeOS needs these files for animated WebM avatars.
              'sources': [
                '<(libvpx_src_dir)/libmkv/EbmlIDs.h',
                '<(libvpx_src_dir)/libmkv/EbmlWriter.c',
                '<(libvpx_src_dir)/libmkv/EbmlWriter.h',
              ],
            }],
          ],
        },
      ],
    }],
  ],
  'targets': [
    {
      # A tool that runs on host to tract integers from object file.
      'target_name': 'libvpx_obj_int_extract',
      'type': 'executable',
      'toolsets': ['host'],
      'include_dirs': [
        'source/config/<(OS_CATEGORY)/<(target_arch_full)',
        '<(libvpx_src_dir)',
      ],
      'sources': [
        '<(libvpx_src_dir)/build/make/obj_int_extract.c',
      ]
    },
    {
      # A library that contains assembly offsets needed.
      'target_name': 'libvpx_asm_offsets',
      'type': 'static_library',
      'hard_dependency': 1,
      'include_dirs': [
        'source/config/<(OS_CATEGORY)/<(target_arch_full)',
        '<(libvpx_src_dir)',
      ],
      'conditions': [
        ['asan==1', {
          'cflags!': [ '-faddress-sanitizer', ],
          'xcode_settings': {
            'OTHER_CFLAGS!': [ '-faddress-sanitizer', ],
          },
          'ldflags!': [ '-faddress-sanitizer', ],
        }],
      ],
      'sources': [
        '<(shared_generated_dir)/vpx_rtcd.h',
        '<(libvpx_src_dir)/vp8/common/asm_com_offsets.c',
        '<(libvpx_src_dir)/vp8/decoder/asm_dec_offsets.c',
        '<(libvpx_src_dir)/vp8/encoder/asm_enc_offsets.c',
      ],
    },
    {
      # A target that takes assembly offsets library and generate the
      # corresponding assembly files.
      # This target is a hard dependency because the generated .asm files
      # are needed all assembly optimized files in libvpx.
      'target_name': 'gen_asm_offsets',
      'type': 'none',
      'hard_dependency': 1,
      'dependencies': [
        'libvpx_asm_offsets',
        'libvpx_obj_int_extract#host',
      ],
      'conditions': [
        ['OS=="win"', {
          'variables': {
            'ninja_obj_dir': '<(PRODUCT_DIR)/obj/third_party/libvpx/<(libvpx_src_dir)/vp8',
          },
          'actions': [
            {
              'action_name': 'copy_enc_offsets_obj',
              'inputs': [ 'copy_obj.sh' ],
              'outputs': [ '<(INTERMEDIATE_DIR)/asm_enc_offsets.obj' ],
              'action': [
                '<(DEPTH)/third_party/libvpx/copy_obj.sh',
                '-d', '<@(_outputs)',
                '-s', '<(PRODUCT_DIR)/obj/libvpx_asm_offsets/asm_enc_offsets.obj',
                '-s', '<(ninja_obj_dir)/encoder/libvpx_asm_offsets.asm_enc_offsets.obj',
              ],
              'process_output_as_sources': 1,
            },
            {
              'action_name': 'copy_dec_offsets_obj',
              'inputs': [ 'copy_obj.sh' ],
              'outputs': [ '<(INTERMEDIATE_DIR)/asm_dec_offsets.obj' ],
              'action': [
                '<(DEPTH)/third_party/libvpx/copy_obj.sh',
                '-d', '<@(_outputs)',
                '-s', '<(PRODUCT_DIR)/obj/libvpx_asm_offsets/asm_dec_offsets.obj',
                '-s', '<(ninja_obj_dir)/decoder/libvpx_asm_offsets.asm_dec_offsets.obj',
              ],
              'process_output_as_sources': 1,
            },
            {
              'action_name': 'copy_com_offsets_obj',
              'inputs': [ 'copy_obj.sh' ],
              'outputs': [ '<(INTERMEDIATE_DIR)/asm_com_offsets.obj' ],
              'action': [
                '<(DEPTH)/third_party/libvpx/copy_obj.sh',
                '-d', '<@(_outputs)',
                '-s', '<(PRODUCT_DIR)/obj/libvpx_asm_offsets/asm_com_offsets.obj',
                '-s', '<(ninja_obj_dir)/common/libvpx_asm_offsets.asm_com_offsets.obj',
              ],
              'process_output_as_sources': 1,
            },
          ],
          'sources': [
            '<(INTERMEDIATE_DIR)/asm_com_offsets.obj',
            '<(INTERMEDIATE_DIR)/asm_dec_offsets.obj',
            '<(INTERMEDIATE_DIR)/asm_enc_offsets.obj',
          ],
        }, {
          'actions': [
            {
              # Take archived .a file and unpack it unto .o files.
              'action_name': 'unpack_lib_posix',
              'inputs': [
                'unpack_lib_posix.sh',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/asm_com_offsets.o',
                '<(INTERMEDIATE_DIR)/asm_dec_offsets.o',
                '<(INTERMEDIATE_DIR)/asm_enc_offsets.o',
              ],
              'action': [
                '<(DEPTH)/third_party/libvpx/unpack_lib_posix.sh',
                '-d', '<(INTERMEDIATE_DIR)',
                '-a', '<(LIB_DIR)/libvpx_asm_offsets.a',
                '-a', '<(LIB_DIR)/third_party/libvpx/libvpx_asm_offsets.a',
                '-f', 'asm_com_offsets.o',
                '-f', 'asm_dec_offsets.o',
                '-f', 'asm_enc_offsets.o',
              ],
              'process_output_as_sources': 1,
            },
          ],
          # Need this otherwise gyp won't run the rule on them.
          'sources': [
            '<(INTERMEDIATE_DIR)/asm_com_offsets.o',
            '<(INTERMEDIATE_DIR)/asm_dec_offsets.o',
            '<(INTERMEDIATE_DIR)/asm_enc_offsets.o',
          ],
        }],
      ],
      'rules': [
        {
          # Rule to extract integer values for each symbol from an object file.
          'rule_name': 'obj_int_extract',
          'extension': '<(asm_obj_extension)',
          'inputs': [
            '<(PRODUCT_DIR)/libvpx_obj_int_extract',
            'obj_int_extract.sh',
          ],
          'outputs': [
            '<(shared_generated_dir)/<(RULE_INPUT_ROOT).asm',
          ],
          'variables': {
            'conditions': [
              ['target_arch=="arm"', {
                'asm_format': 'gas',
              }, {
                'asm_format': 'rvds',
              }],
            ],
          },
          'action': [
            '<(DEPTH)/third_party/libvpx/obj_int_extract.sh',
            '-e', '<(PRODUCT_DIR)/libvpx_obj_int_extract',
            '-f', '<(asm_format)',
            '-b', '<(RULE_INPUT_PATH)',
            '-o', '<(shared_generated_dir)/<(RULE_INPUT_ROOT).asm',
          ],
          'message': 'Generate assembly offsets <(RULE_INPUT_PATH).',
        },
      ],
    },
    {
      'target_name': 'simple_encoder',
      'type': 'executable',
      'dependencies': [
        'libvpx',
      ],

      # Copy the script to the output folder so that we can use it with
      # absolute path.
      'copies': [{
        'destination': '<(shared_generated_dir)/simple_encoder',
        'files': [
          '<(libvpx_src_dir)/examples/gen_example_code.sh',
        ],
      }],

      # Rule to convert .txt files to .c files.
      'rules': [
        {
          'rule_name': 'generate_example',
          'extension': 'txt',
          'inputs': [ '<(shared_generated_dir)/simple_encoder/gen_example_code.sh', ],
          'outputs': [
            '<(shared_generated_dir)/<(RULE_INPUT_ROOT).c',
          ],
          'action': [
            'bash',
            '-c',
            '<(shared_generated_dir)/simple_encoder/gen_example_code.sh <(RULE_INPUT_PATH) > <(shared_generated_dir)/<(RULE_INPUT_ROOT).c',
          ],
          'process_outputs_as_sources': 1,
          'message': 'Generate libvpx example code <(RULE_INPUT_PATH).',
        },
      ],
      'sources': [
        '<(libvpx_src_dir)/examples/simple_encoder.txt',
      ]
    },
    {
      'target_name': 'simple_decoder',
      'type': 'executable',
      'dependencies': [
        'libvpx',
      ],

      # Copy the script to the output folder so that we can use it with
      # absolute path.
      'copies': [{
        'destination': '<(shared_generated_dir)/simple_decoder',
        'files': [
          '<(libvpx_src_dir)/examples/gen_example_code.sh',
        ],
      }],

      # Rule to convert .txt files to .c files.
      'rules': [
        {
          'rule_name': 'generate_example',
          'extension': 'txt',
          'inputs': [ '<(shared_generated_dir)/simple_decoder/gen_example_code.sh', ],
          'outputs': [
            '<(shared_generated_dir)/<(RULE_INPUT_ROOT).c',
          ],
          'action': [
            'bash',
            '-c',
            '<(shared_generated_dir)/simple_decoder/gen_example_code.sh <(RULE_INPUT_PATH) > <(shared_generated_dir)/<(RULE_INPUT_ROOT).c',
          ],
          'process_outputs_as_sources': 1,
          'message': 'Generate libvpx example code <(RULE_INPUT_PATH).',
        },
      ],
      'sources': [
        '<(libvpx_src_dir)/examples/simple_decoder.txt',
      ]
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
