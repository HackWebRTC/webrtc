#!/usr/bin/env python
# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import subprocess
import sys


def run_ant_build_command(path_to_ant_build_file):
  """Tries to build the passed build file with ant."""
  process = subprocess.Popen([
      'ant', '-buildfile', '%s' % path_to_ant_build_file],
      stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  output, error = process.communicate()
  if process.returncode != 0:
    print 'Error: ', error
  else:
    print output


def _main():
  run_ant_build_command('third_party/zxing/core/build.xml')
  run_ant_build_command('third_party/zxing/javase/build.xml')
  return 0


if __name__ == '__main__':
  sys.exit(_main())