#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Downloads the appengine SDK from WebRTC storage and unpacks it.

Requires that depot_tools is installed and in the PATH.

It downloads compressed files in the directory where the script lives.
This is because the precondition is that the script lives in the same
directory of the .sha1 files.
"""

import glob
import os
import sys
import subprocess

import utils


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

def _DownloadResources(dir_to_scan_for_sha1):
  print 'Downloading files in %s...' % dir_to_scan_for_sha1

  extension = 'bat' if 'win32' in sys.platform else 'py'
  cmd = ['download_from_google_storage.%s' % extension,
         '--bucket=chromium-webrtc-resources',
         '--directory', dir_to_scan_for_sha1]
  subprocess.check_call(cmd)


def _StripVersionNumberFromMercurialFolder(output_dir):
  unpacked_name = glob.glob(os.path.join(output_dir, 'mercurial*'))
  assert len(unpacked_name) == 1, 'Should have precisely one mercurial!'
  os.rename(unpacked_name[0], os.path.join(output_dir, 'mercurial'))


def main(argv):
  if len(argv) == 1:
    return 'Usage: %s <output_dir>' % argv[0]

  output_dir = argv[1]
  appengine_zip_path = os.path.join(SCRIPT_DIR, 'google-appengine.zip')
  old_appengine_sha1 = utils.ComputeSHA1(appengine_zip_path)

  mercurial_tar_path = os.path.join(SCRIPT_DIR, 'mercurial-src.tar.gz')
  old_mercurial_sha1 = utils.ComputeSHA1(mercurial_tar_path)

  apprtc_zip_path = os.path.join(SCRIPT_DIR, 'prebuilt_apprtc.zip')
  old_apprtc_sha1 = utils.ComputeSHA1(apprtc_zip_path)

  _DownloadResources(SCRIPT_DIR)

  if old_appengine_sha1 != utils.ComputeSHA1(appengine_zip_path):
    utils.RemoveDirectory(os.path.join(output_dir, 'google_appengine'))
    utils.UnpackArchiveTo(appengine_zip_path, output_dir)

  if old_mercurial_sha1 != utils.ComputeSHA1(mercurial_tar_path):
    utils.RemoveDirectory(os.path.join(output_dir, 'mercurial'))
    utils.UnpackArchiveTo(mercurial_tar_path, output_dir)
    _StripVersionNumberFromMercurialFolder(output_dir)

  if old_apprtc_sha1 != utils.ComputeSHA1(apprtc_zip_path):
    utils.RemoveDirectory(os.path.join(output_dir, 'apprtc'))
    utils.UnpackArchiveTo(apprtc_zip_path, output_dir)

if __name__ == '__main__':
  sys.exit(main(sys.argv))
