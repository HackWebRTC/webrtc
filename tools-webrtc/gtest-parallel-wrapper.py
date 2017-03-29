#!/usr/bin/env python

# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""
This script acts as an interface between the Chromium infrastructure and
gtest-parallel, renaming options and translating environment variables into
flags. Developers should execute gtest-parallel directly.

In particular, this translates the GTEST_SHARD_INDEX and GTEST_TOTAL_SHARDS
environment variables to the --shard_index and --shard_count flags, and renames
the --isolated-script-test-output flag to --dump_json_test_results.

Note that the flags unprocessed by this script will passed as arguments to the
test executable, i.e.

  gtest-parallel-wrapper.py some_test \
      --isolated-script-test-output=some_dir \
      --unprocessed_arg_1
      -- \
      --unprocessed_arg_2

will be converted into

  python gtest-parallel some_test \
      --shard_count 1 \
      --shard_index 0 \
      --dump_json_test_results some_dir \
      -- \
      --unprocessed_arg_1
      --unprocessed_arg_2
"""

import argparse
import os
import subprocess
import sys

# GTEST_SHARD_INDEX and GTEST_TOTAL_SHARDS must be removed from the environment
# otherwise it will be picked up by the binary, causing a bug where only tests
# in the firsh shard are executed.
test_env = os.environ.copy()
gtest_shard_index = test_env.pop('GTEST_SHARD_INDEX', '0')
gtest_total_shards = test_env.pop('GTEST_TOTAL_SHARDS', '1')

webrtc_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
gtest_parallel_path = os.path.join(webrtc_root, 'third_party', 'gtest-parallel',
                                   'gtest-parallel')

# Ignore '--'. Options unprocessed by this script will be passed to the test as
# arguments.
if '--' in sys.argv:
  del sys.argv[sys.argv.index('--')]

parser = argparse.ArgumentParser()
parser.add_argument('--isolated-script-test-output', type=str, default=None)
parser.add_argument('--output_dir', type=str, default=None)
parser.add_argument('--timeout', type=int, default=None)

options, unprocessed = parser.parse_known_args()
test_executable = unprocessed[0]
test_arguments = unprocessed[1:]

gtest_args = [
    test_executable,
    '--shard_count',
    gtest_total_shards,
    '--shard_index',
    gtest_shard_index,
]

# --isolated-script-test-output is used to upload results to the flakiness
# dashboard. This translation is made because gtest-parallel expects the flag to
# be called --dump_json_test_results instead.
if options.isolated_script_test_output:
  gtest_args += [
      '--dump_json_test_results',
      options.isolated_script_test_output,
  ]

if options.output_dir:
  gtest_args += [
      '--output_dir',
      options.output_dir,
  ]

if options.timeout:
  gtest_args += [
      '--timeout',
      str(options.timeout),
  ]

command = [
    sys.executable,
    gtest_parallel_path,
] + gtest_args + ['--'] + test_arguments

print 'gtest-parallel-wrapper: Executing command %s' % ' '.join(command)
sys.stdout.flush()

sys.exit(subprocess.call(command, env=test_env, cwd=os.getcwd()))
