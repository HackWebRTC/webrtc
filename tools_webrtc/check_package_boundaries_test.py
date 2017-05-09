#!/usr/bin/env python

# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import ast
import os
import unittest

from check_package_boundaries import CheckPackageBoundaries


MSG_FORMAT = 'ERROR:check_package_boundaries.py: Unexpected %s.'
TESTDATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                            'testdata')


def ReadPylFile(file_path):
  with open(file_path) as f:
    return ast.literal_eval(f.read())


class Logger(object):
  def __init__(self, test_dir):
    self.messages = []
    self.test_dir = test_dir

  def Log(self, build_file_path, line_number, target_name, source_file,
          subpackage):
    build_file_path = os.path.relpath(build_file_path, self.test_dir)
    build_file_path = build_file_path.replace(os.path.sep, '/')
    self.messages.append([build_file_path, line_number, target_name,
                          source_file, subpackage])


class UnitTest(unittest.TestCase):
  def RunTest(self, test_dir, check_all_build_files=False):
    logger = Logger(test_dir)
    build_files = [os.path.join(test_dir, 'BUILD.gn')]
    if check_all_build_files:
      build_files = None
    CheckPackageBoundaries(test_dir, logger, build_files)
    expected_messages = ReadPylFile(os.path.join(test_dir, 'expected.pyl'))
    self.assertListEqual(sorted(expected_messages), sorted(logger.messages))

  def testNoErrors(self):
    self.RunTest(os.path.join(TESTDATA_DIR, 'no_errors'))

  def testMultipleErrorsSingleTarget(self):
    self.RunTest(os.path.join(TESTDATA_DIR, 'multiple_errors_single_target'))

  def testMultipleErrorsMultipleTargets(self):
    self.RunTest(os.path.join(TESTDATA_DIR, 'multiple_errors_multiple_targets'))

  def testCommonPrefix(self):
    self.RunTest(os.path.join(TESTDATA_DIR, 'common_prefix'))

  def testAllBuildFiles(self):
    self.RunTest(os.path.join(TESTDATA_DIR, 'all_build_files'), True)


if __name__ == '__main__':
  unittest.main()
