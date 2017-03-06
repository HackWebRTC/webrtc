#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""
This script is the wrapper that runs the low-bandwidth audio test.

After running the test, post-process steps for calculating audio quality of the
output files will be performed.
"""

import argparse
import logging
import os
import subprocess
import sys


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, os.pardir, os.pardir,
                                        os.pardir))


def _RunCommand(argv, cwd=SRC_DIR, **kwargs):
  logging.info('Running %r', argv)
  subprocess.check_call(argv, cwd=cwd, **kwargs)


def _ParseArgs():
  parser = argparse.ArgumentParser(description='Run low-bandwidth audio tests.')
  parser.add_argument('build_dir',
      help='Path to the build directory (e.g. out/Release).')
  args = parser.parse_args()
  return args


def main():
  # pylint: disable=W0101
  logging.basicConfig(level=logging.INFO)

  args = _ParseArgs()

  test_executable = os.path.join(args.build_dir, 'low_bandwidth_audio_test')
  if sys.platform == 'win32':
    test_executable += '.exe'

  _RunCommand([test_executable])


if __name__ == '__main__':
  sys.exit(main())
