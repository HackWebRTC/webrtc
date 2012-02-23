#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

__author__ = "ivinnichenko@webrtc.org (Illya Vinnichenko)"

from optparse import OptionParser
import os
import sys
import time

# The path is considered whitelisted if any of these entries appear
# at some point in the path
WHITELIST = ["buildbot.tac", "master.cfg", "public_html", "changes.pck",
             "webrtc_buildbot"]


def is_whitelisted(path):
  """Check if file is whitelisted.

    path: file path.
  """
  for entry in WHITELIST:
    if entry in path:
      return True
  return False


def remove_old_filenames(path, num_days, verbose):
  """Remove old files.

    path: base directory for removal.
    num_days: days limit for removal.
    verbose: print every cmd?
  """
  print "Cleaning up everything in %s older than %s days" % (path, num_days)
  current_time = time.time()
  limit = 60*60*24*num_days
  for root, unused_dirs, files in os.walk(path):
    for filename in files:
      current_file = os.path.join(root, filename)
      if is_whitelisted(current_file):
        continue
      time_stamp = os.stat(current_file).st_mtime
      if (current_time - time_stamp) > limit:
        str_stamp = time.strftime("%a, %d %b %Y %H:%M:%S +0000",
                    time.gmtime(time_stamp))
        if verbose:
          print "Removing [%s], stamped on %s" % (current_file, str_stamp)
        os.remove(current_file)


def main():
  usage = "usage: %prog [options] arg"
  parser = OptionParser(usage)
  parser.add_option("-p", "--path", dest="cleanup_path", help="base directory")
  parser.add_option("-n", "--num_days", dest="num_days", help="number of days")
  parser.add_option("-q", "--quiet",
                    action="store_false", dest="verbose", default=True,
                    help="don't print status messages to stdout")

  options, args = parser.parse_args()
  if not options.cleanup_path:
    print "You must specify base directory"
    sys.exit(2)
  if not options.num_days:
    print "You must specify number of days old"
    sys.exit(2)
  remove_old_filenames(options.cleanup_path, int(options.num_days),
                       options.verbose)

if __name__ == "__main__":
  main()
