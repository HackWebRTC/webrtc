#!/usr/bin/env python
# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import os
import subprocess
import sys


def run_ant_build_command(path_to_ant_build_file):
  """Tries to build the passed build file with ant."""
  ant_suffix = '.bat' if 'win32' in sys.platform else ''
  cmd = ['ant%s' % ant_suffix, '-buildfile', path_to_ant_build_file]
  try:
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()
    if process.returncode != 0:
      print 'Failed to execute: %s\nError: %s' % (' '.join(cmd), stderr)
    else:
      print stdout
  except Exception as e:
    print 'Failed to execute: %s\nError: %s' % (' '.join(cmd), e)


def _main():
  core_build = os.path.join('third_party', 'zxing', 'core', 'build.xml')
  run_ant_build_command(core_build)

  javase_build = os.path.join('third_party', 'zxing', 'javase', 'build.xml')
  run_ant_build_command(javase_build)
  return 0


if __name__ == '__main__':
  sys.exit(_main())