#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

import os
import sys


def main():
  """
  Performs changes after checkout needed for WebRTC buildbot customizations.

  This script performs the following tasks:
  - Adds an import of the WebRTC slave_utils module in the buildbot.tac file.
    It will add a comment and the import at the end of the file, if it's not
    already present.
  - Removes the slaves.cfg for the Libvpx waterfall on Windows platforms, since
    symbolic links are not available on this platform and the resulting link
    file causes a parsing error in Python when loaded during slave startup.

  Using this script, we don't need to maintain our own version of the slave
  scripts and can automatically stay up to date with their changes.

  This script should be invoked as a hooks step in the DEPS file, like this:
  hooks = [
   {
      # Update slave buildbot.tac to include WebRTC slave_utils import.
      "pattern": ".",
      "action": ["python", "tools/fix_webrtc_buildbots.py"],
    },
  ]
  """
  SCRIPT_PATH = os.path.dirname(os.path.abspath(__file__))

  # Patch buildbot.tac.
  TARGET_FILE = os.path.join(SCRIPT_PATH,
                             'continuous_build/build/slave/buildbot.tac')
  COMMENT_LINE = '# Load WebRTC custom slave script.\n'
  IMPORT_LINE = 'from webrtc_buildbot import slave_utils\n'

  file = open(TARGET_FILE, 'r')
  if file.read().find(IMPORT_LINE) == -1:
    print 'Patching %s with WebRTC imports.' % TARGET_FILE
    file.close()
    file = open(TARGET_FILE, 'a')
    file.write(COMMENT_LINE)
    file.write(IMPORT_LINE)
  file.close()

  # Remove Libvpx waterfall's slaves.cfg on Windows.
  if sys.platform.startswith('win'):
    slave_cfg = os.path.join(SCRIPT_PATH, ('continuous_build/build_internal/'
                                           'masters/master.libvpx/slaves.cfg'))
    if os.path.exists(slave_cfg):
      os.remove(slave_cfg)
      print 'Removed %s for Libvpx waterfall on Windows.' % slave_cfg

if __name__ == '__main__':
  main()
