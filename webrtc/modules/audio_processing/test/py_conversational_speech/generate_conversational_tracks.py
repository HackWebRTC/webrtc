#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Generate multiple-end audio tracks to simulate conversational
   speech with two or more participants.

Usage: generate_conversational_tracks.py
          -i path/to/source/audiotracks
          -t path/to/timing_file.txt
          -o output/path
"""

import argparse
import logging
import sys

def _InstanceArgumentsParser():
  parser = argparse.ArgumentParser(description=(
      'Generate multiple-end audio tracks to simulate conversational speech '
      'with two or more participants.'))

  parser.add_argument('-i', '--input_tracks_path', required=True,
                      help='directory containing the speech turn wav files')

  parser.add_argument('-t', '--timing_file', required=True,
                      help='path to the timing text file')

  parser.add_argument('-o', '--output_dir', required=False,
                      help=('base path to the output directory in which the '
                            'output wav files are saved'),
                      default='output')

  return parser


def main():
  # TODO(alessiob): level = logging.INFO once debugged.
  logging.basicConfig(level=logging.DEBUG)

  parser = _InstanceArgumentsParser()
  args = parser.parse_args()

  # TODO(alessiob): pass the arguments to the app controller.

  # TODO(alessiob): remove when comment above addressed.
  logging.debug(args)

  sys.exit(0)


if __name__ == '__main__':
  main()
