#!/usr/bin/env python
# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Downloads precompiled tools.

These are checked into the repository as SHA-1 hashes (see *.sha1 files in
subdirectories). Note that chrome-webrtc-resources is a Google-internal bucket,
so please download and compile these tools manually if this script fails.
"""

import os
import subprocess
import sys


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def main(directories):
  if not directories:
    directories = [SCRIPT_DIR]

  for path in directories:
    cmd = [
      'download_from_google_storage',
      '--directory',
      '--num_threads=10',
      '--bucket', 'chrome-webrtc-resources',
      '--auto_platform',
      '--recursive',
      path,
    ]
    print 'Downloading precompiled tools...'
    subprocess.check_call(cmd)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
