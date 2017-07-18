#!/usr/bin/env python

# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# pylint: disable=invalid-name
"""
This script acts as an interface between the Chromium infrastructure and
gtest-parallel, renaming options and translating environment variables into
flags. Developers should execute gtest-parallel directly.

In particular, this translates the GTEST_SHARD_INDEX and GTEST_TOTAL_SHARDS
environment variables to the --shard_index and --shard_count flags, and renames
the --isolated-script-test-output flag to --dump_json_test_results.

All flags before '--' will be passed as arguments to gtest-parallel, and
(almost) all flags after '--' will be passed as arguments to the test
executable.
The exception is that --isolated-script-test-output and
--isolated-script-test-chartson-output are expected to be after '--', so they
are processed and removed from there.
For example:

  gtest-parallel-wrapper.py some_test \
      --some_flag=some_value \
      --another_flag \
      -- \
      --isolated-script-test-output=some_dir \
      --isolated-script-test-chartjson-output=some_other_dir \
      --foo=bar \
      --baz

Will be converted into:

  python gtest-parallel some_test \
      --shard_count 1 \
      --shard_index 0 \
      --some_flag=some_value \
      --another_flag \
      --dump_json_test_results=some_dir \
      -- \
      --foo=bar
      --baz

"""

import argparse
import os
import subprocess
import sys


def CatFiles(file_list, output_file):
  with open(output_file, 'w') as output_file:
    for filename in file_list:
      with open(filename) as input_file:
        output_file.write(input_file.read())
      os.remove(filename)


def get_args_and_env():
  if '--' in sys.argv:
    argv_index = sys.argv.index('--')
  else:
    argv_index = len(sys.argv)

  gtest_parallel_args = sys.argv[1:argv_index]
  executable_args = sys.argv[argv_index + 1:]

  parser = argparse.ArgumentParser()
  parser.add_argument('--isolated-script-test-output', type=str, default=None)

  # We don't need to implement this flag, and possibly can't, since it's
  # intended for results of Telemetry tests. See
  # https://chromium.googlesource.com/external/github.com/catapult-project/catapult/+/HEAD/dashboard/docs/data-format.md
  parser.add_argument('--isolated-script-test-chartjson-output', type=str,
                      default=None)

  # We have to do this, since --isolated-script-test-output is passed as an
  # argument to the executable by the swarming scripts, and we want to pass it
  # to gtest-parallel instead.
  options, executable_args = parser.parse_known_args(executable_args)

  # --isolated-script-test-output is used to upload results to the flakiness
  # dashboard. This translation is made because gtest-parallel expects the flag
  # to be called --dump_json_test_results instead.
  if options.isolated_script_test_output:
    gtest_parallel_args += [
        '--dump_json_test_results',
        options.isolated_script_test_output,
    ]

  # GTEST_SHARD_INDEX and GTEST_TOTAL_SHARDS must be removed from the
  # environment. Otherwise it will be picked up by the binary, causing a bug
  # where only tests in the first shard are executed.
  test_env = os.environ.copy()
  gtest_shard_index = test_env.pop('GTEST_SHARD_INDEX', '0')
  gtest_total_shards = test_env.pop('GTEST_TOTAL_SHARDS', '1')

  gtest_parallel_args += [
      '--shard_count',
      gtest_total_shards,
      '--shard_index',
      gtest_shard_index,
  ] + ['--'] + executable_args

  return gtest_parallel_args, test_env


def get_output_dir(gtest_parallel_args):
  parser = argparse.ArgumentParser()
  parser.add_argument('--output_dir', type=str, default=None)
  options, _ = parser.parse_known_args(gtest_parallel_args)
  return options.output_dir


def main():
  webrtc_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
  gtest_parallel_path = os.path.join(
      webrtc_root, 'third_party', 'gtest-parallel', 'gtest-parallel')

  gtest_parallel_args, test_env = get_args_and_env()

  command = [
      sys.executable,
      gtest_parallel_path,
  ] + gtest_parallel_args

  print 'gtest-parallel-wrapper: Executing command %s' % ' '.join(command)
  sys.stdout.flush()

  exit_code = subprocess.call(command, env=test_env, cwd=os.getcwd())

  output_dir = get_output_dir(gtest_parallel_args)
  if output_dir:
    for test_status in 'passed', 'failed', 'interrupted':
      logs_dir = os.path.join(output_dir, test_status)
      if not os.path.isdir(logs_dir):
        continue
      logs = [os.path.join(logs_dir, log) for log in os.listdir(logs_dir)]
      log_file = os.path.join(output_dir, '%s-tests.log' % test_status)
      CatFiles(logs, log_file)
      os.rmdir(logs_dir)

  return exit_code


if __name__ == '__main__':
  sys.exit(main())
