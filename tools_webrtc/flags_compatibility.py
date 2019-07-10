#!/usr/bin/env python

# Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import argparse
import logging
import subprocess
import sys

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--isolated-script-test-output')
  parser.add_argument('--isolated-script-test-perf-output')
  args, unrecognized_args = parser.parse_known_args()

  test_command = unrecognized_args
  if args.isolated_script_test_output:
    test_command += ['--isolated_script_test_output',
                     args.isolated_script_test_output]
  if args.isolated_script_test_perf_output:
    test_command += ['--isolated_script_test_perf_output',
                     args.isolated_script_test_perf_output]
  logging.info('Running %r', test_command)

  return subprocess.call(test_command)


if __name__ == '__main__':
  # pylint: disable=W0101
  logging.basicConfig(level=logging.INFO)
  sys.exit(main())
