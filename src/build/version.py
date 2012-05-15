#!/usr/bin/env python

# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Get svn revision of working copy

This script tries to get the svn revision as much as it can. It supports
both git-svn and svn. It will fail if not in a git-svn or svn repository;
in this case the script will return "n/a".
"""

__author__ = 'leozwang@webrtc.org (Leo Wang)'

import shlex
import sys

def popen_cmd_and_get_output(cmd):
  """Return (status, output) of executing cmd in a shell."""
  import subprocess

  pipe = subprocess.Popen(shlex.split(cmd),
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  stdout, stderr = pipe.communicate()
  return pipe.returncode, stdout

def get_git_svn_revision(line):
  """Return revision number in git-svn output, set to "n/a" if it fails."""
  words = line.split()
  for index, word in enumerate(words):
    if word == "Revision:":
      return words[index+1]
  return "n/a"

if __name__ == '__main__':
  (gitstatus, gitresult) = popen_cmd_and_get_output("git svn info")
  if gitstatus == 0:
    result = get_git_svn_revision(gitresult)
    print result
  else:
    (svnstatus, svnresult) = popen_cmd_and_get_output("svnversion")
    if svnstatus == 0:
      print svnresult
    else:
      print "n/a"

  sys.exit(0)
