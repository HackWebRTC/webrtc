#!/usr/bin/python

#  Copyright 2016 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Script for exporting iOS header files."""

import errno
import sys

import argparse
import os
import shutil

LEGACY_HEADER_DIRS = ['talk/app/webrtc/objc/public', 'webrtc/base/objc/']
HEADER_DIRS = ['webrtc/api/objc/', 'webrtc/base/objc/',
               'webrtc/modules/audio_device/ios/objc']
# Individual header files that should also be exported.
LEGACY_HEADER_INCLUDES = []
HEADER_INCLUDES = []
# Individual header files that should not be exported.
LEGACY_HEADER_EXCLUDES = ['talk/app/webrtc/objc/public/RTCNSGLVideoView.h']
HEADER_EXCLUDES = [
    'webrtc/api/objc/avfoundationvideocapturer.h',
    'webrtc/api/objc/RTCNSGLVideoView.h',
    'webrtc/api/objc/RTCVideoRendererAdapter.h',
    'webrtc/base/objc/NSString+StdString.h',
    'webrtc/base/objc/RTCUIApplication.h',
    'webrtc/modules/audio_device/ios/objc/RTCAudioSessionDelegateAdapter.h',
]


def ExportHeaders(include_base_dir, use_legacy_headers):
  """Exports iOS header files.

  Creates an include directory and recreates the hierarchy for the header files
  within the include directory.

  Args:
    include_base_dir: directory where the include directory should be created
    use_legacy_headers: whether or not to export the old headers
  """

  include_dir_name = 'include'
  include_path = os.path.join(include_base_dir, include_dir_name)
  # Remove existing directory first in case files change.
  if os.path.exists(include_path):
    shutil.rmtree(include_path)

  script_path = sys.path[0]
  webrtc_base_path = os.path.join(script_path, '../../..')

  header_dirs = HEADER_DIRS
  include_headers = HEADER_INCLUDES
  exclude_headers = HEADER_EXCLUDES
  if use_legacy_headers:
    header_dirs = LEGACY_HEADER_DIRS
    include_headers = LEGACY_HEADER_INCLUDES
    exclude_headers = LEGACY_HEADER_EXCLUDES

  for directory in header_dirs:
    full_dir_path = os.path.join(webrtc_base_path, directory)
    filenames = os.listdir(full_dir_path)
    for filename in filenames:
      if filename.endswith('.h') and not filename.endswith('+Private.h'):
        include_headers.append(os.path.join(directory, filename))

  for header in exclude_headers:
    include_headers.remove(header)

  for header_path in include_headers:
    output_dir = os.path.join(include_path, os.path.dirname(header_path))
    # Create hierarchy for the header file within the include directory.
    try:
      os.makedirs(output_dir)
    except OSError as exc:
      if exc.errno != errno.EEXIST:
        raise exc
    current_path = os.path.join(webrtc_base_path, header_path)
    new_path = os.path.join(include_path, header_path)
    shutil.copy(current_path, new_path)


def Main():
  parser_description = 'Export WebRTC ObjC API headers.'
  parser = argparse.ArgumentParser(description=parser_description)
  parser.add_argument('output_dir',
                      help='Output directory to write headers to.',
                      type=str)
  parser.add_argument('use_legacy_headers',
                      help='Reads the old headers instead of the current ones.',
                      type=int)
  args = parser.parse_args()
  use_legacy_headers = args.use_legacy_headers != 0
  output_dir = args.output_dir
  ExportHeaders(output_dir, use_legacy_headers)


if __name__ == '__main__':
  sys.exit(Main())
