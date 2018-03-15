# Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import os
import shutil
import subprocess
import sys
import tempfile
from contextlib import contextmanager


# The module find_depot_tools lives into the ./build directory.
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, os.pardir, os.pardir))
sys.path.append(os.path.join(SRC_DIR, 'build'))
import find_depot_tools


@contextmanager
def DefaultGnProject():
  """Generates a GN projects with defaul args and returns the path to it."""
  out_dir = tempfile.mkdtemp('gn')
  gen_command = [
    sys.executable,
    os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'gn.py'),
    'gen',
    out_dir,
  ]
  subprocess.check_output(gen_command)
  try:
    yield out_dir
  finally:
    shutil.rmtree(out_dir, ignore_errors=True)


def BelongsToTestTarget(file_path, out_dir):
  """Returns True if file_path is part of a testonly build target.

  This function relies on 'gn refs'. It actually runs:
  $ gn refs <out_dir> file_path --testonly=true`

  Which returns a list of build targets containing file_path (well, only
  one build target should own a file, but that is not a strict rule).

  If the list is empty, it means the file is owned by a non-testonly build
  target and the function will return False. Otherwise it will return True.

  Args:
    file_path: string with the local path of the file to analyze.
    out_dir: the path to the GN out directory to analyze.

  Returns:
    boolean: True if the file belongs to a testonly build target.
  """
  refs_command = [
    sys.executable,
    os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'gn.py'),
    'refs',
    out_dir,
    file_path,
    '--testonly=true'
  ]
  output = subprocess.check_output(refs_command)
  # If file_path belongs to a test target, output will contain a list of them.
  return output != ''
