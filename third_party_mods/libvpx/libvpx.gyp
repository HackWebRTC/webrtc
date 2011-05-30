# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'libvpx',
      'type': 'static_library',
      # Don't build yasm from source on Windows
      'conditions': [
        ['OS!="win"', {
          'dependencies': [
            '../yasm/yasm.gyp:yasm#host',
          ],
        },
        ],
      ],
      'variables': {
        'shared_generated_dir':
          '<(SHARED_INTERMEDIATE_DIR)/third_party/libvpx',
        'yasm_path': '<(PRODUCT_DIR)/yasm',
        'yasm_flags': [
          '-I', 'config/<(OS)/<(target_arch)',
          '-I', '.'
        ],
        'conditions': [
          ['OS!="win"', {
              'asm_obj_dir':
                '<(shared_generated_dir)',
              'obj_file_ending':
                'o',
            },
            {
              'asm_obj_dir':
                'asm',
              'obj_file_ending':
                'obj',
              'yasm_path': '../yasm/binaries/win/yasm.exe',
            }
          ],
          ['target_arch=="ia32"', {
            'conditions': [
              ['OS=="linux"', {
                'yasm_flags': [
                  '-felf32',
                ],
              },
              ],
              ['OS=="mac"', {
               'yasm_flags': [
                  '-fmacho32',
                ],
              },
              ],
              ['OS=="win"', {
                'yasm_flags': [
                  '-fwin32',
                ],
              },
              ],
            ],
            'yasm_flags': [
              '-m', 'x86',
            ],
          },
          ],
          ['target_arch=="x64"', {
            'conditions': [
              ['OS=="linux"', {
                'yasm_flags': [
                  '-felf64',
               ],
             },
             ],
             ['OS=="mac"', {
               'yasm_flags': [
                  '-fmacho64',
                ],
             },
             ],
             ['OS=="win"', {
               'yasm_flags': [
                 '-win64',
               ],
             },
             ],
            ],
            'yasm_flags': [
              '-m', 'amd64',
            ],
          },
          ],
        ],
      },
      'include_dirs': [
        'config/<(OS)/<(target_arch)',
        'build',
        '.',
        'vp8/common',
        'vp8/decoder',
        'vp8/encoder',
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

      'sources': [
        'vpx/src/vpx_decoder.c',
        'vpx/src/vpx_decoder_compat.c',
        'vpx/src/vpx_encoder.c',
        'vpx/src/vpx_codec.c',
        'vpx/src/vpx_image.c',
        'vpx_mem/vpx_mem.c',
        'vpx_scale/generic/vpxscale.c',
        'vpx_scale/generic/yv12config.c',
        'vpx_scale/generic/yv12extend.c',
        'vpx_scale/generic/scalesystemdependant.c',
        'vpx_scale/generic/gen_scalers.c',
        'vp8/common/alloccommon.c',
        'vp8/common/blockd.c',
        'vp8/common/debugmodes.c',
        'vp8/common/entropy.c',
        'vp8/common/entropymode.c',
        'vp8/common/entropymv.c',
        'vp8/common/extend.c',
        'vp8/common/filter.c',
        'vp8/common/findnearmv.c',
        'vp8/common/generic/systemdependent.c',
        'vp8/common/idctllm.c',
        'vp8/common/invtrans.c',
        'vp8/common/loopfilter.c',
        'vp8/common/loopfilter_filters.c',
        'vp8/common/mbpitch.c',
        'vp8/common/modecont.c',
        'vp8/common/modecontext.c',
        'vp8/common/postproc.c',
        'vp8/common/quant_common.c',
        'vp8/common/recon.c',
        'vp8/common/reconinter.c',
        'vp8/common/reconintra.c',
        'vp8/common/reconintra4x4.c',
        'vp8/common/setupintrarecon.c',
        'vp8/common/swapyv12buffer.c',
        'vp8/common/textblit.c',
        'vp8/common/treecoder.c',
        'vp8/common/x86/x86_systemdependent.c',
        'vp8/common/x86/vp8_asm_stubs.c',
        'vp8/common/x86/loopfilter_x86.c',
        'vp8/vp8_cx_iface.c',
        'vp8/encoder/bitstream.c',
        'vp8/encoder/boolhuff.c',
        'vp8/encoder/dct.c',
        'vp8/encoder/encodeframe.c',
        'vp8/encoder/encodeintra.c',
        'vp8/encoder/encodemb.c',
        'vp8/encoder/encodemv.c',
        'vp8/encoder/ethreading.c',
        'vp8/encoder/firstpass.c',
        'vp8/encoder/generic/csystemdependent.c',
        'vp8/encoder/mcomp.c',
        'vp8/encoder/modecosts.c',
        'vp8/encoder/onyx_if.c',
        'vp8/encoder/pickinter.c',
        'vp8/encoder/picklpf.c',
        'vp8/encoder/psnr.c',
        'vp8/encoder/quantize.c',
        'vp8/encoder/ratectrl.c',
        'vp8/encoder/rdopt.c',
        'vp8/encoder/sad_c.c',
        'vp8/encoder/segmentation.c',
        'vp8/encoder/tokenize.c',
        'vp8/encoder/treewriter.c',
        'vp8/encoder/variance_c.c',
        'vp8/encoder/temporal_filter.c',
        'vp8/encoder/x86/x86_csystemdependent.c',
        'vp8/encoder/x86/variance_mmx.c',
        'vp8/encoder/x86/variance_sse2.c',
        'vp8/vp8_dx_iface.c',
        'vp8/decoder/dboolhuff.c',
        'vp8/decoder/decodemv.c',
        'vp8/decoder/decodframe.c',
        'vp8/decoder/dequantize.c',
        'vp8/decoder/detokenize.c',
        'vp8/decoder/generic/dsystemdependent.c',
        'vp8/decoder/onyxd_if.c',
        'vp8/decoder/threading.c',
        'vp8/decoder/idct_blk.c',
        'vp8/decoder/reconintra_mt.c',
        'vp8/decoder/x86/x86_dsystemdependent.c',
        'vp8/decoder/x86/idct_blk_mmx.c',
        'vp8/decoder/x86/idct_blk_sse2.c',
        'vpx_ports/x86_cpuid.c',
        # Yasm inputs.
        'vp8/common/x86/idctllm_mmx.asm',
        'vp8/common/x86/idctllm_sse2.asm',
        'vp8/common/x86/iwalsh_mmx.asm',
        'vp8/common/x86/iwalsh_sse2.asm',
        'vp8/common/x86/loopfilter_mmx.asm',
        'vp8/common/x86/loopfilter_sse2.asm',
        'vp8/common/x86/postproc_mmx.asm',
        'vp8/common/x86/postproc_sse2.asm',
        'vp8/common/x86/recon_mmx.asm',
        'vp8/common/x86/recon_sse2.asm',
        'vp8/common/x86/subpixel_mmx.asm',
        'vp8/common/x86/subpixel_sse2.asm',
        'vp8/common/x86/subpixel_ssse3.asm',
        'vp8/decoder/x86/dequantize_mmx.asm',
        'vp8/encoder/x86/dct_mmx.asm',
        'vp8/encoder/x86/dct_sse2.asm',
        'vp8/encoder/x86/encodeopt.asm',
        'vp8/encoder/x86/fwalsh_sse2.asm',
        'vp8/encoder/x86/quantize_mmx.asm',
        'vp8/encoder/x86/quantize_sse2.asm',
        'vp8/encoder/x86/quantize_ssse3.asm',
        'vp8/encoder/x86/sad_mmx.asm',
        'vp8/encoder/x86/sad_sse2.asm',
        'vp8/encoder/x86/sad_sse3.asm',
        'vp8/encoder/x86/sad_sse4.asm',
        'vp8/encoder/x86/sad_ssse3.asm',
        'vp8/encoder/x86/subtract_mmx.asm',
        'vp8/encoder/x86/subtract_sse2.asm',
        'vp8/encoder/x86/temporal_filter_apply_sse2.asm',
        'vp8/encoder/x86/variance_impl_mmx.asm',
        'vp8/encoder/x86/variance_impl_sse2.asm',
        'vpx_ports/emms.asm',
        'vpx_ports/x86_abi_support.asm',

        # Generated by ./configure and checked in.
        'config/<(OS)/<(target_arch)/vpx_config.c',
      ]
    }
  ]
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
