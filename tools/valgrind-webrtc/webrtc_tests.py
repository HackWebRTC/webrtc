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
'''

import optparse
import sys

import logging_utils

import chrome_tests

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

  # Performs the deferred-argument black magic described in the usage.
  translated_args = map(lambda arg: arg.replace('+', '-'), args)

  for t in options.test:
    tests = chrome_tests.ChromeTests(options, translated_args, t)
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
  sys.argv.insert(sys.argv.index('-t') + 1, 'cmdline')
  print sys.argv
  ret = _main(sys.argv)
  sys.exit(ret)