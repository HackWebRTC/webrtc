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
import re
import subprocess
import sys


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, os.pardir, os.pardir,
                                        os.pardir))


def _LogCommand(command):
  logging.info('Running %r', command)
  return command


def _ParseArgs():
  parser = argparse.ArgumentParser(description='Run low-bandwidth audio tests.')
  parser.add_argument('build_dir',
      help='Path to the build directory (e.g. out/Release).')
  parser.add_argument('--remove', action='store_true',
      help='Remove output audio files after testing.')
  args = parser.parse_args()
  return args


def _GetPlatform():
  if sys.platform == 'win32':
    return 'win'
  elif sys.platform == 'darwin':
    return 'mac'
  elif sys.platform.startswith('linux'):
    return 'linux'


def _GetExecutableExtension():
  if sys.platform == 'win32':
    return '.exe'
  else:
    return ''


def _DownloadTools():
  tools_dir = os.path.join(SRC_DIR, 'tools-webrtc')
  toolchain_dir = os.path.join(tools_dir, 'audio_quality')

  # Download pesq.
  download_script = os.path.join(tools_dir, 'download_tools.py')
  command = [sys.executable, download_script, toolchain_dir]
  subprocess.check_call(_LogCommand(command))

  pesq_path = os.path.join(toolchain_dir, _GetPlatform(),
                           'pesq' + _GetExecutableExtension())
  return pesq_path


def main():
  # pylint: disable=W0101
  logging.basicConfig(level=logging.INFO)

  args = _ParseArgs()

  pesq_path = _DownloadTools()

  test_executable_path = os.path.join(args.build_dir,
      'low_bandwidth_audio_test' + _GetExecutableExtension())

  # Start the test executable that produces audio files.
  command = [test_executable_path]
  test_process = subprocess.Popen(_LogCommand(command), stdout=subprocess.PIPE)

  for line in iter(test_process.stdout.readline, ''):
    # Echo the output to screen.
    sys.stdout.write(line)

    # Extract specific lines that contain information about produced files.
    match = re.search(r'^TEST (\w+) ([^:]+?):([^:]+?)\n?$', line)
    if not match:
      continue
    test_name, reference_file, degraded_file = match.groups()

    # Analyze audio
    command = [pesq_path, '+16000', reference_file, degraded_file]
    pesq_output = subprocess.check_output(_LogCommand(command))

    if args.remove:
      os.remove(degraded_file)

    # Find the scores in stdout of pesq.
    match = re.search(
        r'Prediction \(Raw MOS, MOS-LQO\):\s+=\s+([\d.]+)\s+([\d.]+)',
        pesq_output)
    if match:
      raw_mos, _ = match.groups()

      # Output a result for the perf dashboard.
      print 'RESULT pesq_mos: %s= %s score' % (test_name, raw_mos)
    else:
      logging.error('PESQ: %s', pesq_output.splitlines()[-1])

  return test_process.wait()


if __name__ == '__main__':
  sys.exit(main())
