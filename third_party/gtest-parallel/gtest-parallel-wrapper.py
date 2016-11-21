#!/usr/bin/env python

# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import os
import subprocess
import sys

# GTEST_SHARD_INDEX and GTEST_TOTAL_SHARDS must be removed from the environment
# otherwise it will be picked up by the binary, causing a bug where only tests
# in the firsh shard are executed.
test_env = os.environ.copy()
gtest_shard_index = test_env.pop('GTEST_SHARD_INDEX', '0')
gtest_total_shards = test_env.pop('GTEST_TOTAL_SHARDS', '1')

gtest_parallel_path = os.path.dirname(os.path.abspath(__file__))
gtest_parallel_path = os.path.join(gtest_parallel_path, 'gtest-parallel')

command = [
    sys.executable,
    gtest_parallel_path,
    '--shard_count',
    gtest_total_shards,
    '--shard_index',
    gtest_shard_index,
] + sys.argv[1:]

print 'gtest-parallel-wrapper: Executing command %s' % ' '.join(command)
sys.stdout.flush()

sys.exit(subprocess.call(command, env=test_env, cwd=os.getcwd()))
