#!/usr/bin/env python

# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# Used to work around an upstream bug (crbug.com/644722).
# This file will be removed ASAP.

import os
import subprocess
import sys

# Path to the corrupt project.
MOCKITO_CHECKOUT_PATH = 'src/third_party/mockito/src'


def main():
  # If this checkout exists, run `git remote prune origin` from it.
  if os.path.isdir(MOCKITO_CHECKOUT_PATH):
    subprocess.check_output(
        ['git', 'remote', 'prune', 'origin'], cwd=MOCKITO_CHECKOUT_PATH)
  else:
    sys.stdout.write('{} does not exist!\n'.format(MOCKITO_CHECKOUT_PATH))
  return 0


if __name__ == '__main__':
  sys.exit(main())
