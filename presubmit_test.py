#!/usr/bin/env python

#  Copyright 2017 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

import os
import shutil
import tempfile
import textwrap
import unittest

import PRESUBMIT
from presubmit_test_mocks import MockInputApi, MockOutputApi, MockFile


class CheckBugEntryField(unittest.TestCase):
  def testCommitMessageBugEntryWithNoError(self):
    mock_input_api = MockInputApi()
    mock_output_api = MockOutputApi()
    mock_input_api.change.BUG = 'webrtc:1234'
    errors = PRESUBMIT.CheckCommitMessageBugEntry(mock_input_api,
                                                  mock_output_api)
    self.assertEqual(0, len(errors))

  def testCommitMessageBugEntryReturnError(self):
    mock_input_api = MockInputApi()
    mock_output_api = MockOutputApi()
    mock_input_api.change.BUG = 'webrtc:1234,webrtc=4321'
    errors = PRESUBMIT.CheckCommitMessageBugEntry(mock_input_api,
                                                  mock_output_api)
    self.assertEqual(1, len(errors))
    self.assertEqual(('Bogus BUG entry: webrtc=4321. Please specify'
                      ' the issue tracker prefix and the issue number,'
                      ' separated by a colon, e.g. webrtc:123 or'
                      ' chromium:12345.'), str(errors[0]))

  def testCommitMessageBugEntryIsNone(self):
    mock_input_api = MockInputApi()
    mock_output_api = MockOutputApi()
    mock_input_api.change.BUG = 'None'
    errors = PRESUBMIT.CheckCommitMessageBugEntry(mock_input_api,
                                                  mock_output_api)
    self.assertEqual(0, len(errors))


class CheckNewlineAtTheEndOfProtoFiles(unittest.TestCase):

  def setUp(self):
    self.tmp_dir = tempfile.mkdtemp()
    self.proto_file_path = os.path.join(self.tmp_dir, 'foo.proto')
    self.input_api = MockInputApi()
    self.output_api = MockOutputApi()

  def tearDown(self):
    shutil.rmtree(self.tmp_dir, ignore_errors=True)

  def testErrorIfProtoFileDoesNotEndWithNewline(self):
    self.__GenerateProtoWithoutNewlineAtTheEnd()
    self.input_api.files = [MockFile(self.proto_file_path)]
    errors = PRESUBMIT.CheckNewlineAtTheEndOfProtoFiles(self.input_api,
                                                        self.output_api)
    self.assertEqual(1, len(errors))
    self.assertEqual(
        'File %s must end with exactly one newline.' % self.proto_file_path,
        str(errors[0]))

  def testNoErrorIfProtoFileEndsWithNewline(self):
    self.__GenerateProtoWithNewlineAtTheEnd()
    self.input_api.files = [MockFile(self.proto_file_path)]
    errors = PRESUBMIT.CheckNewlineAtTheEndOfProtoFiles(self.input_api,
                                                        self.output_api)
    self.assertEqual(0, len(errors))

  def __GenerateProtoWithNewlineAtTheEnd(self):
    with open(self.proto_file_path, 'w') as f:
      f.write(textwrap.dedent("""
        syntax = "proto2";
        option optimize_for = LITE_RUNTIME;
        package webrtc.audioproc;
      """))

  def __GenerateProtoWithoutNewlineAtTheEnd(self):
    with open(self.proto_file_path, 'w') as f:
      f.write(textwrap.dedent("""
        syntax = "proto2";
        option optimize_for = LITE_RUNTIME;
        package webrtc.audioproc;"""))


if __name__ == '__main__':
  unittest.main()
