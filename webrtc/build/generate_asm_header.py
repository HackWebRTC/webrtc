#!/usr/bin/env python
#
# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""This script is a tool to generate special header files from input
C source files.

It first assembles the input source files to generate intermediate assembly
files (*.s). Then it parses the .s files and finds declarations of variables
whose names start with the string specified as the third argument in the
command-line, translates the variable names and values into constant defines
and writes them into header files.
"""

import os
import sys
import subprocess
from optparse import OptionParser

def main(argv):
  parser = OptionParser()
  usage = 'Usage: %prog [options] input_file'
  parser.set_usage(usage)
  parser.add_option('--compiler', default = 'gcc', help = 'compiler name')
  parser.add_option('--options', default = '-S', help = 'compiler options')
  parser.add_option('--pattern', default = 'offset_', help = 'A match pattern'
                    ' used for searching the relevant constants.')
  parser.add_option('--dir', default = '.', help = 'output directory')
  (options, args) = parser.parse_args()

  # Generate complete intermediate and header file names.
  input_file_name = os.path.basename(args[0])
  file_base_name = os.path.splitext(input_file_name)[0]
  interim_file = options.dir + "/" + file_base_name + '.s'
  out_file = interim_file.replace('.s', '.h')

  # Set the shell command with the compiler and options inputs.
  compiler_command = (options.compiler + " " + options.options + " " + args[0]
      + " -o " + interim_file)
  # Run the shell command and generate the intermediate file.
  subprocess.check_call(compiler_command, shell=True)

  infile = open(interim_file)  # The intermediate file.
  outfile = open(out_file, 'w')  # The output header file.

  # Generate the output header file.
  for line in infile:  # Iterate though all the lines in the input file.
    if line.startswith(options.pattern):
      outfile.write('#define ')
      outfile.write(line.split(':')[0])  # Write the constant name.
      outfile.write(' ')
    if line.find('.word') >= 0:
      outfile.write(line.split('.word')[1])  # Write the constant value.

  infile.close()
  outfile.close()

if __name__ == "__main__":
  main(sys.argv[1:])
