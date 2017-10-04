#!/usr/bin/env python

# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import argparse
import logging
import os
import re
import sys


DISPLAY_LEVEL = 1
IGNORE_LEVEL = 0

# TARGET_RE matches a GN target, and extracts the target name and the contents.
TARGET_RE = re.compile(r'\d+\$(?P<indent>\s*)\w+\("(?P<target_name>\w+)"\) {'
                       r'(?P<target_contents>.*?)'
                       r'\d+\$(?P=indent)}',
                       re.MULTILINE | re.DOTALL)

# SOURCES_RE matches a block of sources inside a GN target.
SOURCES_RE = re.compile(r'sources \+?= \[(?P<sources>.*?)\]',
                        re.MULTILINE | re.DOTALL)

LOG_FORMAT = '%(message)s'
ERROR_MESSAGE = ("{}:{} in target '{}':\n"
                 "  Source file '{}'\n"
                 "  crosses boundary of package '{}'.\n")


class Logger(object):
  def __init__(self, messages_left=None):
    self.log_level = DISPLAY_LEVEL
    self.messages_left = messages_left

  def Log(self, build_file_path, line_number, target_name, source_file,
          subpackage):
    if self.messages_left is not None:
      if not self.messages_left:
        self.log_level = IGNORE_LEVEL
      else:
        self.messages_left -= 1
    message = ERROR_MESSAGE.format(build_file_path, line_number, target_name,
                                   source_file, subpackage)
    logging.log(self.log_level, message)


def _BuildSubpackagesPattern(packages, query):
  """Returns a regular expression that matches source files inside subpackages
  of the given query."""
  query += os.path.sep
  length = len(query)
  pattern = r'(?P<line_number>\d+)\$\s*"(?P<source_file>(?P<subpackage>'
  pattern += '|'.join(re.escape(package[length:].replace(os.path.sep, '/'))
                      for package in packages if package.startswith(query))
  pattern += r')/[\w\./]*)"'
  return re.compile(pattern)


def _ReadFileAndPrependLines(file_path):
  """Reads the contents of a file and prepends the line number to every line."""
  with open(file_path) as f:
    return "".join("{}${}".format(line_number, line)
                   for line_number, line in enumerate(f, 1))


def _CheckBuildFile(build_file_path, packages, logger):
  """Iterates oven all the targets of the given BUILD.gn file, and verifies that
  the source files referenced by it don't belong to any of it's subpackages.
  Returns True if a package boundary violation was found.
  """
  found_violations = False
  package = os.path.dirname(build_file_path)
  subpackages_re = _BuildSubpackagesPattern(packages, package)

  build_file_contents = _ReadFileAndPrependLines(build_file_path)
  for target_match in TARGET_RE.finditer(build_file_contents):
    target_name = target_match.group('target_name')
    target_contents = target_match.group('target_contents')
    for sources_match in SOURCES_RE.finditer(target_contents):
      sources = sources_match.group('sources')
      for subpackages_match in subpackages_re.finditer(sources):
        subpackage = subpackages_match.group('subpackage')
        source_file = subpackages_match.group('source_file')
        line_number = subpackages_match.group('line_number')
        if subpackage:
          found_violations = True
          logger.Log(build_file_path, line_number, target_name, source_file,
                     subpackage)

  return found_violations


def CheckPackageBoundaries(root_dir, logger, build_files=None):
  packages = [root for root, _, files in os.walk(root_dir)
              if 'BUILD.gn' in files]

  if build_files is not None:
    for build_file_path in build_files:
      assert build_file_path.startswith(root_dir)
  else:
    build_files = [os.path.join(package, 'BUILD.gn') for package in packages]

  return any([_CheckBuildFile(build_file_path, packages, logger)
              for build_file_path in build_files])


def main():
  parser = argparse.ArgumentParser(
      description='Script that checks package boundary violations in GN '
                  'build files.')

  parser.add_argument('root_dir', metavar='ROOT_DIR',
                      help='The root directory that contains all BUILD.gn '
                           'files to be processed.')
  parser.add_argument('build_files', metavar='BUILD_FILE', nargs='*',
                      help='A list of BUILD.gn files to be processed. If no '
                           'files are given, all BUILD.gn files under ROOT_DIR '
                           'will be processed.')
  parser.add_argument('--max_messages', type=int, default=None,
                      help='If set, the maximum number of violations to be '
                           'displayed.')

  args = parser.parse_args()

  logging.basicConfig(format=LOG_FORMAT)
  logging.getLogger().setLevel(DISPLAY_LEVEL)
  logger = Logger(args.max_messages)

  return CheckPackageBoundaries(args.root_dir, logger, args.build_files)


if __name__ == '__main__':
  sys.exit(main())
