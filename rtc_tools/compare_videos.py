#!/usr/bin/env python
# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import optparse
import os
import subprocess
import sys


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# Chrome browsertests will throw away stderr; avoid that output gets lost.
sys.stderr = sys.stdout


def _ParseArgs():
  """Registers the command-line options."""
  usage = 'usage: %prog [options]'
  parser = optparse.OptionParser(usage=usage)

  parser.add_option('--label', type='string', default='MY_TEST',
                    help=('Label of the test, used to identify different '
                          'tests. Default: %default'))
  parser.add_option('--ref_video', type='string',
                    help='Reference video to compare with (YUV).')
  parser.add_option('--test_video', type='string',
                    help=('Test video to be compared with the reference '
                          'video (YUV).'))
  parser.add_option('--frame_analyzer', type='string',
                    help='Path to the frame analyzer executable.')
  parser.add_option('--barcode_decoder', type='string',
                    help=('DEPRECATED'))
  parser.add_option('--ffmpeg_path', type='string',
                    help=('DEPRECATED'))
  parser.add_option('--zxing_path', type='string',
                    help=('DEPRECATED'))
  parser.add_option('--stats_file_ref', type='string', default='stats_ref.txt',
                    help=('DEPRECATED'))
  parser.add_option('--stats_file_test', type='string',
                    help=('DEPRECATED'))
  parser.add_option('--stats_file', type='string',
                    help=('DEPRECATED'))
  parser.add_option('--yuv_frame_width', type='int', default=640,
                    help=('DEPRECATED'))
  parser.add_option('--yuv_frame_height', type='int', default=480,
                    help=('DEPRECATED'))
  parser.add_option('--chartjson_result_file', type='str', default=None,
                    help='Where to store perf results in chartjson format.')
  options, _ = parser.parse_args()

  if not options.ref_video:
    parser.error('You must provide a path to the reference video!')
  if not os.path.exists(options.ref_video):
    parser.error('Cannot find the reference video at %s' % options.ref_video)

  if not options.test_video:
    parser.error('You must provide a path to the test video!')
  if not os.path.exists(options.test_video):
    parser.error('Cannot find the test video at %s' % options.test_video)

  if not options.frame_analyzer:
    parser.error('You must provide the path to the frame analyzer executable!')
  if not os.path.exists(options.frame_analyzer):
    parser.error('Cannot find frame analyzer executable at %s!' %
                 options.frame_analyzer)
  return options

def _DevNull():
  """On Windows, sometimes the inherited stdin handle from the parent process
  fails. Workaround this by passing null to stdin to the subprocesses commands.
  This function can be used to create the null file handler.
  """
  return open(os.devnull, 'r')

def main():
  """The main function.

  A simple invocation is:
  ./webrtc/rtc_tools/compare_videos.py
  --ref_video=<path_and_name_of_reference_video>
  --test_video=<path_and_name_of_test_video>
  --frame_analyzer=<path_and_name_of_the_frame_analyzer_executable>
  """
  options = _ParseArgs()

  # Run frame analyzer to compare the videos and print output.
  cmd = [
    options.frame_analyzer,
    '--label=%s' % options.label,
    '--reference_file=%s' % options.ref_video,
    '--test_file=%s' % options.test_video,
  ]
  if options.chartjson_result_file:
    cmd.append('--chartjson_result_file=%s' % options.chartjson_result_file)
  frame_analyzer = subprocess.Popen(cmd, stdin=_DevNull(),
                                    stdout=sys.stdout, stderr=sys.stderr)
  frame_analyzer.wait()
  if frame_analyzer.returncode != 0:
    print 'Failed to run frame analyzer.'
    return 1

  return 0

if __name__ == '__main__':
  sys.exit(main())
