#!/usr/bin/env python
#
# libjingle
# Copyright 2004--2010, Google Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#  1. Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright notice,
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.
#  3. The name of the author may not be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
# EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Runs various libjingle tests through valgrind_test.py.

This script inherits the chrome_tests.py in Chrome, but allows running any test
instead of only the hard-coded ones. It uses the -t cmdline flag to do this, and
only supports specifying a single test for each run.

Suppression files:
The Chrome valgrind directory we use as a DEPS dependency contains the following
suppression files:
  valgrind/memcheck/suppressions.txt
  valgrind/memcheck/suppressions_mac.txt
  valgrind/tsan/suppressions.txt
  valgrind/tsan/suppressions_mac.txt
  valgrind/tsan/suppressions_win32.txt
Since they're referenced from the chrome_tests.py script, we have similar files
below the directory of this script. When executing, this script will setup both
Chrome's suppression files and our own, so we can easily maintain libjingle
specific suppressions in our own files.
"""

import logging
import optparse
import os
import sys

import logging_utils
import path_utils

import chrome_tests


class LibjingleTest(chrome_tests.ChromeTests):
  """Class that handles setup of suppressions for libjingle.

  Everything else is inherited from chrome_tests.ChromeTests.
  """

  def __init__(self, test_name, options, args, test_in_chrome_tests):
    """Create a Libjingle test.
    Args:
      test_name: Short name for the test executable (no path).
      options: options to pass to ChromeTests.
      args: args to pass to ChromeTests.
      test_in_chrome_tests: The name of the test configuration in ChromeTests.
    """
    self._test_name = test_name
    chrome_tests.ChromeTests.__init__(self, options, args, test_in_chrome_tests)

  def _DefaultCommand(self, tool, exe=None, valgrind_test_args=None):
    """Override command-building method so we can add more suppressions."""
    cmd = chrome_tests.ChromeTests._DefaultCommand(self, tool, exe,
                                                   valgrind_test_args)

    # Add gtest filters, if found.
    chrome_tests.ChromeTests._AppendGtestFilter(self, tool, self._test_name,
                                                cmd)

    # When ChromeTests._DefaultCommand has executed, it has setup suppression
    # files based on what's found in the memcheck/ or tsan/ subdirectories of
    # this script's location. If Mac or Windows is executing, additional
    # platform specific files have also been added.
    # Since only the ones located below this directory are added, we must also
    # add the ones maintained by Chrome, located in ../valgrind.

    # The idea is to look for --suppression arguments in the cmd list and add a
    # modified copy of each suppression file, for the corresponding file in
    # ../valgrind. If we would simply replace 'valgrind-libjingle' with
    # 'valgrind' we may produce invalid paths if other parts of the path contain
    # that string. That's why the code below only replaces the end of the path.
    script_dir = path_utils.ScriptDir()
    old_base, _ = os.path.split(script_dir)
    new_dir = os.path.join(old_base, 'valgrind')
    add_suppressions = []
    for token in cmd:
      if '--suppressions' in token:
        add_suppressions.append(token.replace(script_dir, new_dir))
    return add_suppressions + cmd


def main(_):
  parser = optparse.OptionParser('usage: %prog -b <dir> -t <test> <test args>')
  parser.disable_interspersed_args()
  parser.add_option('-b', '--build_dir',
                    help=('Location of the compiler output. Can only be used '
                          'when the test argument does not contain this path.'))
  parser.add_option('-t', '--test', help='Test to run.')
  parser.add_option('', '--baseline', action='store_true', default=False,
                    help='Generate baseline data instead of validating')
  parser.add_option('', '--gtest_filter',
                    help='Additional arguments to --gtest_filter')
  parser.add_option('', '--gtest_repeat',
                    help='Argument for --gtest_repeat')
  parser.add_option('-v', '--verbose', action='store_true', default=False,
                    help='Verbose output - enable debug log messages')
  parser.add_option('', '--tool', dest='valgrind_tool', default='memcheck',
                    help='Specify a valgrind tool to run the tests under')
  parser.add_option('', '--tool_flags', dest='valgrind_tool_flags', default='',
                    help='Specify custom flags for the selected valgrind tool')
  parser.add_option('', '--keep_logs', action='store_true', default=False,
                    help=('Store memory tool logs in the <tool>.logs directory '
                          'instead of /tmp.\nThis can be useful for tool '
                          'developers/maintainers.\nPlease note that the <tool>'
                          '.logs directory will be clobbered on tool startup.'))
  options, args = parser.parse_args()

  if options.verbose:
    logging_utils.config_root(logging.DEBUG)
  else:
    logging_utils.config_root()

  if not options.test:
    parser.error('--test not specified')

  # If --build_dir is provided, prepend it to the test executable if needed.
  test_executable = options.test
  if options.build_dir and not test_executable.startswith(options.build_dir):
    test_executable = os.path.join(options.build_dir, test_executable)
  args = [test_executable] + args

  test = LibjingleTest(options.test, options, args, 'cmdline')
  return test.Run()

if __name__ == '__main__':
  return_code = main(sys.argv)
  sys.exit(return_code)
