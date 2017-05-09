#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Downloads the golang SDK from WebRTC storage and unpacks it.

Requires that depot_tools is installed and in the PATH.

The precondition is that a directory 'golang' lives in the same directory
as the script.
This 'golang' directory has the following structure:

/golang
 |
 |-----/linux/go.tar.gz.sha1
 |-----/win/go.zip.sha1
 |-----/mac/go.tar.gz.sha1
"""

import os
import sys

import utils


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def _GetGoArchivePathForPlatform():
  archive_extension = 'zip' if utils.GetPlatform() == 'win' else 'tar.gz'
  return os.path.join(utils.GetPlatform(), 'go.%s' % archive_extension)


def main(argv):
  if len(argv) == 1:
    return 'Usage: %s <output_dir>' % argv[0]

  output_dir = os.path.join(argv[1])
  golang_path = os.path.join(SCRIPT_DIR, 'golang')
  archive_path = os.path.join(golang_path, _GetGoArchivePathForPlatform())
  old_archive_sha1 = utils.ComputeSHA1(archive_path)

  utils.DownloadFilesFromGoogleStorage(golang_path)

  if (old_archive_sha1 != utils.ComputeSHA1(archive_path)
      or not os.path.exists('go')):
    utils.RemoveDirectory(os.path.join(output_dir, 'go'))
    utils.UnpackArchiveTo(archive_path, output_dir)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
