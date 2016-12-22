#!/usr/bin/python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for test_runner.py."""

import collections
import json
import os
import sys
import unittest

import test_runner


class TestCase(unittest.TestCase):
  """Test case which supports installing mocks. Uninstalls on tear down."""

  def __init__(self, *args, **kwargs):
    """Initializes a new instance of this class."""
    super(TestCase, self).__init__(*args, **kwargs)

    # Maps object to a dict which maps names of mocked members to their
    # original values.
    self._mocks = collections.OrderedDict()

  def mock(self, obj, member, mock):
    """Installs mock in place of the named member of the given obj.

    Args:
      obj: Any object.
      member: String naming the attribute of the object to mock.
      mock: The mock to install.
    """
    self._mocks.setdefault(obj, collections.OrderedDict()).setdefault(
        member, getattr(obj, member))
    setattr(obj, member, mock)

  def tearDown(self, *args, **kwargs):
    """Uninstalls mocks."""
    super(TestCase, self).tearDown(*args, **kwargs)

    for obj in self._mocks:
      for member, original_value in self._mocks[obj].iteritems():
        setattr(obj, member, original_value)


class GetKIFTestFilterTest(TestCase):
  """Tests for test_runner.get_kif_test_filter."""

  def test_correct(self):
    """Ensures correctness of filter."""
    tests = [
      'KIF.test1',
      'KIF.test2',
    ]
    expected = 'NAME:test1|test2'

    self.assertEqual(test_runner.get_kif_test_filter(tests), expected)

  def test_correct_inverted(self):
    """Ensures correctness of inverted filter."""
    tests = [
      'KIF.test1',
      'KIF.test2',
    ]
    expected = '-NAME:test1|test2'

    self.assertEqual(
        test_runner.get_kif_test_filter(tests, invert=True), expected)


class GetGTestFilterTest(TestCase):
  """Tests for test_runner.get_gtest_filter."""

  def test_correct(self):
    """Ensures correctness of filter."""
    tests = [
      'test.1',
      'test.2',
    ]
    expected = 'test.1:test.2'

    self.assertEqual(test_runner.get_gtest_filter(tests), expected)

  def test_correct_inverted(self):
    """Ensures correctness of inverted filter."""
    tests = [
      'test.1',
      'test.2',
    ]
    expected = '-test.1:test.2'

    self.assertEqual(
        test_runner.get_gtest_filter(tests, invert=True), expected)


class SimulatorTestRunnerTest(TestCase):
  """Tests for test_runner.SimulatorTestRunner."""

  def test_app_not_found(self):
    """Ensures AppNotFoundError is raised."""
    def exists(path):
      if path == 'fake-app':
        return False
      return True

    def find_xcode(version):
      return {'found': True}

    def check_output(command):
      return 'fake-bundle-id'

    self.mock(test_runner.os.path, 'exists', exists)
    self.mock(test_runner.find_xcode, 'find_xcode', find_xcode)
    self.mock(test_runner.subprocess, 'check_output', check_output)

    self.assertRaises(
        test_runner.AppNotFoundError,
        test_runner.SimulatorTestRunner,
        'fake-app',
        'fake-iossim',
        'platform',
        'os',
        'xcode-version',
        'out-dir',
    )

  def test_iossim_not_found(self):
    """Ensures SimulatorNotFoundError is raised."""
    def exists(path):
      if path == 'fake-iossim':
        return False
      return True

    def find_xcode(version):
      return {'found': True}

    def check_output(command):
      return 'fake-bundle-id'

    self.mock(test_runner.os.path, 'exists', exists)
    self.mock(test_runner.find_xcode, 'find_xcode', find_xcode)
    self.mock(test_runner.subprocess, 'check_output', check_output)

    self.assertRaises(
        test_runner.SimulatorNotFoundError,
        test_runner.SimulatorTestRunner,
        'fake-app',
        'fake-iossim',
        'platform',
        'os',
        'xcode-version',
        'out-dir',
    )

  def test_init(self):
    """Ensures instance is created."""
    def exists(path):
      return True

    def find_xcode(version):
      return {'found': True}

    def check_output(command):
      return 'fake-bundle-id'

    self.mock(test_runner.os.path, 'exists', exists)
    self.mock(test_runner.find_xcode, 'find_xcode', find_xcode)
    self.mock(test_runner.subprocess, 'check_output', check_output)

    tr = test_runner.SimulatorTestRunner(
        'fake-app',
        'fake-iossim',
        'platform',
        'os',
        'xcode-version',
        'out-dir',
    )

    self.failUnless(tr)

  def test_startup_crash(self):
    """Ensures test is relaunched once on startup crash."""
    def exists(path):
      return True

    def find_xcode(version):
      return {'found': True}

    def check_output(command):
      return 'fake-bundle-id'

    def set_up(self):
      return

    @staticmethod
    def _run(command):
      return collections.namedtuple('result', ['crashed', 'crashed_test'])(
          crashed=True, crashed_test=None)

    def tear_down(self):
      return

    self.mock(test_runner.os.path, 'exists', exists)
    self.mock(test_runner.find_xcode, 'find_xcode', find_xcode)
    self.mock(test_runner.subprocess, 'check_output', check_output)
    self.mock(test_runner.SimulatorTestRunner, 'set_up', set_up)
    self.mock(test_runner.TestRunner, '_run', _run)
    self.mock(test_runner.SimulatorTestRunner, 'tear_down', tear_down)

    tr = test_runner.SimulatorTestRunner(
        'fake-app',
        'fake-iossim',
        'platform',
        'os',
        'xcode-version',
        'out-dir',
    )
    self.assertRaises(test_runner.AppLaunchError, tr.launch)

  def test_relaunch(self):
    """Ensures test is relaunched on test crash until tests complete."""
    def exists(path):
      return True

    def find_xcode(version):
      return {'found': True}

    def check_output(command):
      return 'fake-bundle-id'

    def set_up(self):
      return

    @staticmethod
    def _run(command):
      result = collections.namedtuple(
          'result', [
              'crashed',
              'crashed_test',
              'failed_tests',
              'flaked_tests',
              'passed_tests',
          ],
      )
      if '-e' not in command:
        # First run, has no test filter supplied. Mock a crash.
        return result(
            crashed=True,
            crashed_test='c',
            failed_tests={'b': ['b-out'], 'c': ['Did not complete.']},
            flaked_tests={'d': ['d-out']},
            passed_tests=['a'],
        )
      else:
        return result(
            crashed=False,
            crashed_test=None,
            failed_tests={},
            flaked_tests={},
            passed_tests=[],
        )

    def tear_down(self):
      return

    self.mock(test_runner.os.path, 'exists', exists)
    self.mock(test_runner.find_xcode, 'find_xcode', find_xcode)
    self.mock(test_runner.subprocess, 'check_output', check_output)
    self.mock(test_runner.SimulatorTestRunner, 'set_up', set_up)
    self.mock(test_runner.TestRunner, '_run', _run)
    self.mock(test_runner.SimulatorTestRunner, 'tear_down', tear_down)

    tr = test_runner.SimulatorTestRunner(
        'fake-app',
        'fake-iossim',
        'platform',
        'os',
        'xcode-version',
        'out-dir',
    )
    tr.launch()
    self.failUnless(tr.logs)


if __name__ == '__main__':
  unittest.main()
