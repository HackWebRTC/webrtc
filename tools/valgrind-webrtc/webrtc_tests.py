#!/usr/bin/env python
# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

'''Runs various WebRTC tests through valgrind_test.py.

This script inherits the chrome_tests.py in Chrome, replacing its tests. We do
this by taking chrome's faux cmdline test and making that the standard, so that
we effectively can pass in any binary we feel like. It's also possible to pass
arguments to the test, provided that the arguments do not contain dashes (these
can be "escaped" by passing + instead, so -a becomes +a, and --my-option becomes
++my_option).

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
Chrome's suppression files and our own, so we can easily maintain WebRTC
specific suppressions in our own files.
'''

import optparse
import os
import sys

import logging_utils
import path_utils

import chrome_tests

class WebRTCTests(chrome_tests.ChromeTests):
  """Class that handles setup of suppressions for WebRTC.

  Everything else is inherited from chrome_tests.ChromeTests.
  """

  def _DefaultCommand(self, tool, exe=None, valgrind_test_args=None):
    """Override command-building method so we can add more suppressions."""
    cmd = chrome_tests.ChromeTests._DefaultCommand(self, tool, exe,
                                                   valgrind_test_args)
    # When ChromeTests._DefaultCommand has executed, it has setup suppression
    # files based on what's found in the memcheck/ or tsan/ subdirectories of
    # this script's location. If Mac or Windows is executing, additional
    # platform specific files have also been added.
    # Since only the ones located below this directory is added, we must also
    # add the ones maintained by Chrome, located in ../valgrind.

    # The idea is to look for --suppression arguments in the cmd list and add a
    # modified copy of each suppression file, for the corresponding file in
    # ../valgrind. If we would simply replace 'valgrind-webrtc' with 'valgrind'
    # we may produce invalid paths if other parts of the path contain that
    # string. That's why the code below only replaces the end of the path.
    old_base, old_dir = _split_script_path()
    new_dir = old_base + 'valgrind'
    add_suppressions = []
    for token in cmd:
      if '--suppressions' in token:
        add_suppressions.append(token.replace(old_base + old_dir, new_dir))
    return add_suppressions + cmd


def _split_script_path():
  """Splits the script's path into a tuple separating the last directory.

    Returns a tuple where the first item is the whole path except the last
    directory and the second item is the name of the last directory.
  """
  script_dir = path_utils.ScriptDir()
  last_sep_index = script_dir.rfind(os.sep)
  return script_dir[0:last_sep_index+1], script_dir[last_sep_index+1:]


def _main(_):
  parser = optparse.OptionParser("usage: %prog -b <dir> -t <test> "
                                 "[-t <test> ...] <arguments to all tests>"
                                 "NOTE: when passing arguments to all tests, "
                                 "      replace any - with +.")
  parser.add_option("-b", "--build_dir",
                    help="the location of the compiler output")
  parser.add_option("-t", "--test", action="append", default=[],
                    help="which test to run, supports test:gtest_filter format "
                         "as well.")
  parser.add_option("", "--baseline", action="store_true", default=False,
                    help="generate baseline data instead of validating")
  parser.add_option("", "--gtest_filter",
                    help="additional arguments to --gtest_filter")
  parser.add_option("", "--gtest_repeat",
                    help="argument for --gtest_repeat")
  parser.add_option("-v", "--verbose", action="store_true", default=False,
                    help="verbose output - enable debug log messages")
  parser.add_option("", "--tool", dest="valgrind_tool", default="memcheck",
                    help="specify a valgrind tool to run the tests under")
  parser.add_option("", "--tool_flags", dest="valgrind_tool_flags", default="",
                    help="specify custom flags for the selected valgrind tool")
  parser.add_option("", "--keep_logs", action="store_true", default=False,
                    help="store memory tool logs in the <tool>.logs directory "
                         "instead of /tmp.\nThis can be useful for tool "
                         "developers/maintainers.\nPlease note that the <tool>"
                         ".logs directory will be clobbered on tool startup.")
  options, args = parser.parse_args()

  if options.verbose:
    logging_utils.config_root(logging.DEBUG)
  else:
    logging_utils.config_root()

  if not options.test:
    parser.error("--test not specified")

  if len(options.test) != 1 and options.gtest_filter:
    parser.error("--gtest_filter and multiple tests don't make sense together")

  # If --build_dir is provided, prepend that path to the test name to make it a
  # valid path when running on the build slaves using Chromium's runtest.py
  if options.build_dir and 'cmdline' in options.test and len(args) == 1:
    args[0] = os.path.join(options.build_dir, args[0])

  # Performs the deferred-argument black magic described in the usage.
  translated_args = map(lambda arg: arg.replace('+', '-'), args)

  for t in options.test:
    tests = WebRTCTests(options, translated_args, t)
    ret = tests.Run()
    if ret: return ret
  return 0

if __name__ == "__main__":
  # Overwrite the ChromeTests tests dictionary. The cmdline option allows the
  # user to pass any executable as parameter to the test script, so we'll use
  # that to get our binaries in (hackish but convenient).
  chrome_tests.ChromeTests._test_list = {
    "cmdline": chrome_tests.ChromeTests.RunCmdLine,
  }

  # We do this so the user can write -t <binary> instead of -t cmdline <binary>.
  if '-t' in sys.argv:
    sys.argv.insert(sys.argv.index('-t') + 1, 'cmdline')
  elif '--test' in sys.argv:
    sys.argv.insert(sys.argv.index('--test') + 1, 'cmdline')

  print sys.argv
  ret = _main(sys.argv)
  sys.exit(ret)
