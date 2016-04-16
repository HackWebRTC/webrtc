#!/usr/bin/python

#  Copyright 2016 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Script for flattening iOS header structure."""

import sys

import argparse
import os
import shutil


def FlattenHeaders(input_dir, output_dir):
  """Flattens iOS header file directory structure."""
  # Create output directories.
  if not os.path.exists(output_dir):
    os.mkdir(output_dir)

  for dirpath, _, filenames in os.walk(input_dir):
    for filename in filenames:
      current_path = os.path.join(dirpath, filename)
      new_path = os.path.join(output_dir, filename)
      shutil.copy(current_path, new_path)


def Main():
  parser_description = 'Flatten WebRTC ObjC API headers.'
  parser = argparse.ArgumentParser(description=parser_description)
  parser.add_argument('input_dir',
                      help='Output directory to write headers to.',
                      type=str)
  parser.add_argument('output_dir',
                      help='Input directory to read headers from.',
                      type=str)
  args = parser.parse_args()
  input_dir = args.input_dir
  output_dir = args.output_dir
  FlattenHeaders(input_dir, output_dir)


if __name__ == '__main__':
  sys.exit(Main())
