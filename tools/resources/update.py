#!/usr/bin/env python
# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

__author__ = 'kjellander@webrtc.org (Henrik Kjellander)'

"""Downloads WebRTC resources files from a remote host."""

from optparse import OptionParser
import os
import shutil
import sys
import tarfile
import tempfile
import urllib2


def main():
  """
  Downloads WebRTC resources files from a remote host.

  This script will download WebRTC resource files used for testing, like audio
  and video files. It will check the current version in the DEPS file and
  compare it with the one downloaded (kept in a text file in the download dir).
  If there's a new version of the resources available it will be downloaded.
  """
  # Constants
  deps_key = 'webrtc_resources_revision'
  remote_url_base = 'http://commondatastorage.googleapis.com/webrtc-resources'
  version_filename = 'webrtc-resources-version'
  filename_prefix = 'webrtc-resources-'
  extension = '.tgz'

  # Variables used by the script
  project_root_dir = os.path.normpath(sys.path[0] + '/../../') 
  deps_file = os.path.join(project_root_dir, 'DEPS')
  downloads_dir = os.path.join(project_root_dir, 'resources')
  current_version_file = os.path.join(downloads_dir, version_filename)
  
  # Ensure the downloads dir is created.
  if not os.path.isdir(downloads_dir):
    os.mkdir(downloads_dir)

  # Define and parse arguments.
  parser = OptionParser()
  parser.add_option('-f', '--force', action='store_true', dest='force', 
                    help='forces download and removes all existing resources.')
  (options, unused_args) = parser.parse_args()

  # Check if we have an existing version already downloaded.
  current_version = 0
  if os.path.isfile(current_version_file):
    f = open(current_version_file)
    current_version = int(f.read())
    f.close()
    print 'Found downloaded resources: version: %s' % current_version

  # Check the DEPS file for the latest version number.
  deps_vars = EvalDepsFile(deps_file)['vars']
  latest_version = int(deps_vars[deps_key])
  print 'Version in DEPS file: %d' % latest_version
  
  # Download archive if forced or latest version is newer than our current.
  if latest_version > current_version or options.force:
    temp_dir = tempfile.mkdtemp(prefix='webrtc-resources-')
    archive_name = '%s%s%s' % (filename_prefix, latest_version, extension)
    remote_archive_url = os.path.join(remote_url_base, archive_name)
    # Download into the temporary directory with display of progress, inspired  
    # by the Stack Overflow post at http://goo.gl/JIrbo
    temp_file = os.path.join(temp_dir, archive_name)
    print 'Downloading: %s' % remote_archive_url
    u = urllib2.urlopen(remote_archive_url)
    f = open(temp_file, 'wb')
    meta = u.info()
    file_size = int(meta.getheaders('Content-Length')[0])
    print 'Progress: %s bytes: %s' % (archive_name, file_size)

    file_size_dl = 0
    block_size = 65536
    while True:
      file_buffer = u.read(block_size)
      if not file_buffer:
        break
      file_size_dl += len(file_buffer)
      f.write(file_buffer)
      status = r'%10d  [%3.2f%%]' % (file_size_dl,
                                     file_size_dl * 100. / file_size)
      status += chr(8) * (len(status) + 1)
      print status,
    print
    f.close()

    # Clean up the existing resources dir.
    print 'Removing old resources in %s' % downloads_dir
    shutil.rmtree(downloads_dir)
    os.mkdir(downloads_dir)

    # Write the latest version to a text file in the resources dir to avoid 
    # re-download of the same version in the future:
    new_version_file = os.path.join(downloads_dir, version_filename)
    f = open(new_version_file, 'w')
    f.write('%d' % latest_version)
    f.close()
  
    # Extract the archive
    archive = tarfile.open(temp_file, 'r:gz')
    archive.extractall(downloads_dir)
    archive.close()
    print 'Extracted resource files into %s' % downloads_dir
    # Clean up the temp dir
    shutil.rmtree(temp_dir)
  else:
    print 'Already have latest (or newer) version: %s' % current_version


def EvalDepsFile(path):
  scope = {'Var': lambda name: scope['vars'][name], 
           'File': lambda name: name}
  execfile(path, {}, scope)
  return scope

if __name__ == '__main__':
  main()
