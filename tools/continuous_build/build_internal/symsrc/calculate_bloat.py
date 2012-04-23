#!/usr/bin/env python
#-*- coding: utf-8 -*-
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

__author__ = 'phoglund@webrtc.org (Patrik Höglund)'

import os
import sys
import subprocess

from optparse import OptionParser

"""Computes a webtreemap-compatible bloat .json file for a binary.

This will produce an overview of the binary which shows the sizes of its
constituent parts. The binary should be built with -g for symbols. If building
Chrome, you must include profiling=1 if building in Release mode.

This script only runs on Linux. It requires the nm utility (part of the binutils
package) as well as the bloat.py script. It can run from any working directory.
"""

THIS_SCRIPTS_PATH = os.path.dirname(os.path.realpath(__file__))
BLOAT_SCRIPT = THIS_SCRIPTS_PATH + '/../../../third_party/bloat/bloat.py'


def _run_nm(binary):
  raw_nm_filename = 'nm.out'
  raw_nm_file = open(raw_nm_filename, 'w')
  subprocess.check_call(['nm', '-C', '-S', '-l', binary], stdout=raw_nm_file)
  raw_nm_file.close()
  return raw_nm_filename


def _run_bloat(raw_nm_filename, source_path, output_filename):
  json_file = open(output_filename, 'w')
  subprocess.check_call([BLOAT_SCRIPT,
                        '--strip-prefix=%s' % source_path,
                        '--nm-output=%s' % raw_nm_filename,
                        'syms'], stdout=json_file, stderr=None)
  json_file.close()


def main():
  if not os.path.exists(BLOAT_SCRIPT):
    return 'Missing required dependency bloat (looked in %s).' % BLOAT_SCRIPT

  usage = 'usage: %prog -b <binary> -s <path to source> -o <output JSON file>'
  parser = OptionParser(usage)
  parser.add_option('-b', '--binary', dest='binary', default=False,
                    help='Binary to run the bloat calculation on. ' +
                         'The binary should be built with -g for symbols.')
  parser.add_option('-s', '--source-path', dest='source_path', default=False,
                    help='Where the binary\'s source code is.')
  parser.add_option('-o', '--output-file', dest='output_file', default=False,
                    help='Where to put the resulting JSON file.')
  options, unused_args = parser.parse_args()

  if not options.binary:
    return '%s\n\nYou must specify the binary to run on.' % usage
  if not options.output_file:
    return '%s\n\nYou must specify where to put the output file.' % usage
  if not options.source_path:
    return '%s\n\nYou must specify the binary\'s source code path.' % usage
  if not os.path.exists(options.binary):
    return 'Binary %s does not exist.' % options.binary
  if not os.path.exists(options.source_path):
    return 'Source path %s does not exist.' % options.source_path

  # Convert the source path to an absolute path. The ending slash is important
  # for --strip-prefix later!
  options.source_path = os.path.realpath(options.source_path) + '/'

  raw_nm_filename = _run_nm(options.binary)
  _run_bloat(raw_nm_filename, options.source_path, options.output_file)

  os.remove(raw_nm_filename)

  return 0

if __name__ == '__main__':
  sys.exit(main())
