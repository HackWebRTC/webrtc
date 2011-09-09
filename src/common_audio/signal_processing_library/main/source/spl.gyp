# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../../../../common_settings.gypi', # Common settings
  ],
  'targets': [
    {
      'target_name': 'spl',
      'type': '<(library)',
      'include_dirs': [
        '../interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
        ],
      },
      'sources': [
        '../interface/signal_processing_library.h',
        '../interface/spl_inl.h',
        'auto_corr_to_refl_coef.c',
        'auto_correlation.c',
        'complex_fft.c',
        'complex_ifft.c',
        'complex_bit_reverse.c',
        'copy_set_operations.c',
        'cos_table.c',
        'cross_correlation.c',
        'division_operations.c',
        'dot_product_with_scale.c',
        'downsample_fast.c',
        'energy.c',
        'filter_ar.c',
        'filter_ar_fast_q12.c',
        'filter_ma_fast_q12.c',
        'get_hanning_window.c',
        'get_scaling_square.c',
        'hanning_table.c',
        'ilbc_specific_functions.c',
        'levinson_durbin.c',
        'lpc_to_refl_coef.c',
        'min_max_operations.c',
        'randn_table.c',
        'randomization_functions.c',
        'refl_coef_to_lpc.c',
        'resample.c',
        'resample_48khz.c',
        'resample_by_2.c',
        'resample_by_2_internal.c',
        'resample_by_2_internal.h',
        'resample_fractional.c',
        'sin_table.c',
        'sin_table_1024.c',
        'spl_sqrt.c',
        'spl_sqrt_floor.c',
        'spl_version.c',
        'splitting_filter.c',
        'sqrt_of_one_minus_x_squared.c',
        'vector_scaling_operations.c',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
