#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Builds a local mercurial (hg) copy.

This is used by the go toolchain.
"""

import os
import subprocess
import sys

import utils


def main(argv):
  if len(argv) != 2:
    return 'Usage: %s <mercurial_dir>' % argv[0]

  mercurial_dir = argv[1]
  if not os.path.exists(mercurial_dir):
    return 'Expected mercurial at {}.'.format(mercurial_dir)

  os.chdir(mercurial_dir)

  if utils.GetPlatform() == 'win':
    subprocess.check_call(['python', 'setup.py', '--pure', 'build_py', '-c',
                           '-d', '.', 'build_ext',
                           '-i', 'build_mo', '--force'])
    with open('hg.bat', 'w') as put_hg_in_path:
      # Write a hg.bat since the go toolchain expects to find something called
      # 'hg' in the path, but Windows only recognizes executables ending with
      # an extension in PATHEXT. Writing hg.bat effectively makes 'hg' callable
      # if the mercurial folder is in PATH.
      mercurial_path = os.path.abspath('hg')
      put_hg_in_path.write('python %s %%*' % mercurial_path)
  else:
    subprocess.check_call(['make', 'local'])

if __name__ == '__main__':
  sys.exit(main(sys.argv))
