# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This script is called from libvpx.gypi. It generates .S files from
# .asm. Converts from ARM syntax used in the RealView Compiler Tools
# (RVCT) to Gnu Assembler Syntax (GAS).

{
  # Copy the script to the output folder so that we can use it with
  # absolute path.
  'copies': [{
    'destination': '<(shared_generated_dir)',
    'files': [
      '<(ads2gas_script_path)',
    ],
  }],
  # Rule to convert .asm files to .S files.
  'rules': [{
    'rule_name': 'convert_asm',
    'extension': 'asm',
    'inputs': [ '<(shared_generated_dir)/<(ads2gas_script)', ],
    'outputs': [
      '<(shared_generated_dir)/<(RULE_INPUT_ROOT).S',
    ],
    'action': [
      'bash',
      '-c',
      'cat <(RULE_INPUT_PATH) |'
        'perl <(shared_generated_dir)/<(ads2gas_script) >'
        '<(shared_generated_dir)/<(RULE_INPUT_ROOT).S',
    ],
    'process_outputs_as_sources': 1,
    'message': 'Convert libvpx asm file for ARM <(RULE_INPUT_PATH).',
  }],
  'variables': {
    # Location of the assembly conversion script.
    'ads2gas_script': 'ads2gas.pl',
    'ads2gas_script_path':
      '<(libvpx_src_dir)/build/make/<(ads2gas_script)',
  },
  'include_dirs': [
    'source/config/<(OS_CATEGORY)/<(target_arch_full)',
    '<(libvpx_src_dir)',
  ],
  'direct_dependent_settings': {
    'include_dirs': [
      '<(libvpx_src_dir)',
    ],
  },
}