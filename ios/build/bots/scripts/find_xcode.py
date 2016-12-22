#!/usr/bin/python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Finds Xcode installations, optionally switching to a desired version.

Usage:
  ./find_xcode.py -j /tmp/out.json -v 6.0.1

  Finds Xcode 6.0.1 and switches to it. Writes a summary to /tmp/out.json
  that includes the Xcode installations that were found, the Xcode version
  that was active before running this script, and the Xcode version that
  is active after running this script.

  e.g. {
    "installations": {
      "/Applications/Xcode5.app": "5.1.1 (5B1008)",
      "/Applications/Xcode6.app": "6.0 (6A313)",
      "/Applications/Xcode6.0.1.app": "6.0.1 (6A317)",
      "/Applications/Xcode6.1.app": "6.1 (6A1046a)",
    },
    "matches": {
      "/Applications/Xcode6.0.1.app": "6.0.1 (6A317)",
    },
    "previous version": {
      "path": "/Application/Xcode5.app",
      "version": "5.1.1",
      "build": "(5B1008)",
    },
    "current version": {
      "path": "/Applications/Xcode6.0.1.app",
      "version": "6.0.1",
      "build": "6A317",
    },
    "found": true,
  }
"""

import argparse
import json
import os
import subprocess
import sys


def get_xcodebuild_path(xcode_app):
  """Returns the path to xcodebuild under the given Xcode app.

  Args:
    xcode_app: The path to an installed Xcode.app. e.g. /Applications/Xcode.app.

  Returns:
    The absolute path to the xcodebuild binary under the given Xcode app.
  """
  return os.path.join(
    xcode_app,
    'Contents',
    'Developer',
    'usr',
    'bin',
    'xcodebuild',
  )


def get_xcode_version(xcodebuild):
  """Returns the Xcode version and build version.

  Args:
    xcodebuild: The absolute path to the xcodebuild binary.

  Returns:
    A tuple of (version string, build version string).
      e.g. ("6.0.1", "6A317")
  """
  # Sample output:
  # Xcode 6.0.1
  # Build version 6A317
  out = subprocess.check_output([xcodebuild, '-version']).splitlines()
  return out[0].split(' ')[-1], out[1].split(' ')[-1]


def get_current_xcode_info():
  """Returns the current Xcode path, version, and build number.

  Returns:
    A dict with 'path', 'version', and 'build' keys.
      'path': The absolute path to the Xcode installation.
      'version': The Xcode version.
      'build': The Xcode build version.
  """
  version, build_version = get_xcode_version('xcodebuild')

  return {
    'path': subprocess.check_output(['xcode-select', '--print-path']).rstrip(),
    'version': version,
    'build': build_version,
  }


def find_xcode(target_version=None):
  """Finds all Xcode versions, switching to the given Xcode version.

  Args:
    target_version: The version of Xcode to switch to, or None if the
      Xcode version should not be switched.

  Returns:
    A summary dict as described in the usage section above.
  """
  xcode_info = {
    'installations': {
    },
    'current version': {
    },
  }

  if target_version:
    xcode_info['found'] = False
    xcode_info['matches'] = {}
    xcode_info['previous version'] = get_current_xcode_info()

    if xcode_info['previous version']['version'] == target_version:
      xcode_info['found'] = True

  for app in os.listdir(os.path.join('/', 'Applications')):
    if app.startswith('Xcode'):
      installation_path = os.path.join('/', 'Applications', app)
      xcodebuild = get_xcodebuild_path(installation_path)

      if os.path.exists(xcodebuild):
        version, build_version = get_xcode_version(xcodebuild)

        xcode_info['installations'][installation_path] = "%s (%s)" % (
          version,
          build_version,
        )

        if target_version and version == target_version:
          xcode_info['matches'][installation_path] = "%s (%s)" % (
            version,
            build_version,
          )

          # If this is the first match, switch to it.
          if not xcode_info['found']:
            subprocess.check_call([
              'sudo',
              'xcode-select',
              '-switch',
              installation_path,
            ])

            xcode_info['found'] = True

  xcode_info['current version'] = get_current_xcode_info()

  if target_version and not xcode_info['found']:
    # Flush buffers to ensure correct output ordering for buildbot.
    sys.stdout.flush()
    sys.stderr.write('Target Xcode version not found: %s\n' % target_version)
    sys.stderr.flush()

  return xcode_info


def main(args):
  xcode_info = find_xcode(args.version)

  if args.json_file:
    with open(args.json_file, 'w') as json_file:
      json.dump(xcode_info, json_file)

  if args.version and not xcode_info['found']:
    return 1

  return 0


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument(
    '-j',
    '--json-file',
    help='Location to write a JSON summary.',
    metavar='file',
  )
  parser.add_argument(
    '-v',
    '--version',
    help='Xcode version to find and switch to.',
    metavar='ver',
  )

  sys.exit(main(parser.parse_args()))
