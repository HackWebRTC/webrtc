#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""
This script is the wrapper that starts a loopback call with stubbed video in
and out. It then analyses the video quality of the output video against the
reference input video.

It expect to be given the webrtc output build directory as the first argument
all other arguments are optional.

It assumes you have a Android device plugged in.
"""

import argparse
import atexit
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import time


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, os.pardir, os.pardir,
                                        os.pardir))
WEBRTC_DEPS_INSTRUCTIONS = """Please add a solution to your .gclient file like
this and run gclient sync:
{
  "name": "webrtc.DEPS",
  "url": "https://chromium.googlesource.com/chromium/deps/webrtc/webrtc.DEPS",
},
"""


class Error(Exception):
  pass


class VideoQualityTestError(Error):
  pass


def _RunCommand(argv, cwd=SRC_DIR, **kwargs):
  logging.info('Running %r', argv)
  subprocess.check_call(argv, cwd=cwd, **kwargs)


def _RunCommandWithOutput(argv, cwd=SRC_DIR, **kwargs):
  logging.info('Running %r', argv)
  return subprocess.check_output(argv, cwd=cwd, **kwargs)


def _RunBackgroundCommand(argv, cwd=SRC_DIR):
  logging.info('Running %r', argv)
  process = subprocess.Popen(argv, cwd=cwd)
  atexit.register(process.terminate)
  time.sleep(0.5)
  status = process.poll()
  if status:  # is not None or 0
    raise subprocess.CalledProcessError(status, argv)
  return process


def _ParseArgs():
  parser = argparse.ArgumentParser(description='Start loopback video analysis.')
  parser.add_argument('build_dir_android',
      help='The path to the build directory for Android.')
  parser.add_argument('--build_dir_x86',
      help='The path to the build directory for building locally.')
  parser.add_argument('--temp_dir',
      help='A temporary directory to put the output.')
  parser.add_argument('--adb-path', help='Path to adb binary.', default='adb')

  args = parser.parse_args()
  return args


def main():
  logging.basicConfig(level=logging.INFO)

  args = _ParseArgs()

  build_dir_android = args.build_dir_android
  build_dir_x86 = args.build_dir_x86
  temp_dir = args.temp_dir
  adb_path = args.adb_path
  if not temp_dir:
    temp_dir = tempfile.mkdtemp()
  else:
    if not os.path.exists(temp_dir):
      os.makedirs(temp_dir)

  if not build_dir_x86:
    build_dir_x86 = os.path.join(temp_dir, 'LocalBuild')
    _RunCommand(['gn', 'gen', build_dir_x86])
    _RunCommand(['ninja', '-C', build_dir_x86, 'frame_analyzer'])

  tools_dir = os.path.join(SRC_DIR, 'tools_webrtc')
  toolchain_dir = os.path.join(tools_dir, 'video_quality_toolchain')

  # Download ffmpeg and zxing.
  download_script = os.path.join(tools_dir, 'download_tools.py')
  _RunCommand([sys.executable, download_script, toolchain_dir])

  # Select an Android device in case multiple are connected
  for line in _RunCommandWithOutput([adb_path, 'devices']).splitlines():
    if line.endswith('\tdevice'):
      android_device = line.split('\t')[0]
      break
  else:
    raise VideoQualityTestError('Cannot find any connected Android device.')

  # Start AppRTC Server
  dev_appserver = os.path.join(SRC_DIR, 'out', 'apprtc', 'google_appengine',
                               'dev_appserver.py')
  if not os.path.isfile(dev_appserver):
    raise VideoQualityTestError('Cannot find %s.\n%s' %
                                (dev_appserver, WEBRTC_DEPS_INSTRUCTIONS))
  appengine_dir = os.path.join(SRC_DIR, 'out', 'apprtc', 'out', 'app_engine')
  _RunBackgroundCommand(['python', dev_appserver, appengine_dir,
                         '--port=9999', '--admin_port=9998',
                         '--skip_sdk_update_check', '--clear_datastore=yes'])

  # Start Collider
  collider_path = os.path.join(SRC_DIR, 'out', 'go-workspace', 'bin',
      'collidermain')
  if not os.path.isfile(collider_path):
    raise VideoQualityTestError('Cannot find %s.\n%s' %
                                (collider_path, WEBRTC_DEPS_INSTRUCTIONS))
  _RunBackgroundCommand([collider_path, '-tls=false',
                         '-port=8089', '-room-server=http://localhost:9999'])

  # Start adb reverse forwarder
  reverseforwarder_path = os.path.join(
      SRC_DIR, 'build', 'android', 'adb_reverse_forwarder.py')
  _RunBackgroundCommand([reverseforwarder_path, '--device', android_device,
                         '9999', '9999', '8089', '8089'])

  # Run the Espresso code.
  test_script = os.path.join(build_dir_android,
      'bin', 'run_AppRTCMobileTestStubbedVideoIO')
  _RunCommand([test_script, '--device', android_device])

  # Pull the output video.
  test_video = os.path.join(temp_dir, 'test_video.y4m')
  _RunCommand([adb_path, '-s', android_device,
               'pull', '/sdcard/output.y4m', test_video])

  test_video_yuv = os.path.join(temp_dir, 'test_video.yuv')

  ffmpeg_path = os.path.join(toolchain_dir, 'linux', 'ffmpeg')

  def ConvertVideo(input_video, output_video):
    _RunCommand([ffmpeg_path, '-y', '-i', input_video, output_video])

  ConvertVideo(test_video, test_video_yuv)

  reference_video = os.path.join(SRC_DIR,
      'resources', 'reference_video_640x360_30fps.y4m')

  reference_video_yuv = os.path.join(temp_dir,
      'reference_video_640x360_30fps.yuv')

  ConvertVideo(reference_video, reference_video_yuv)

  # Run compare script.
  compare_script = os.path.join(SRC_DIR, 'webrtc', 'tools', 'compare_videos.py')
  zxing_path = os.path.join(toolchain_dir, 'linux', 'zxing')

  # The frame_analyzer binary should be built for local computer and not for
  # Android
  frame_analyzer = os.path.join(build_dir_x86, 'frame_analyzer')

  frame_width = 640
  frame_height = 360

  stats_file_ref = os.path.join(temp_dir, 'stats_ref.txt')
  stats_file_test = os.path.join(temp_dir, 'stats_test.txt')

  _RunCommand([
      sys.executable, compare_script, '--ref_video', reference_video_yuv,
      '--test_video', test_video_yuv, '--yuv_frame_width', str(frame_width),
      '--yuv_frame_height', str(frame_height),
      '--stats_file_ref', stats_file_ref,
      '--stats_file_test', stats_file_test, '--frame_analyzer', frame_analyzer,
      '--ffmpeg_path', ffmpeg_path, '--zxing_path', zxing_path])

  shutil.rmtree(temp_dir)


if __name__ == '__main__':
  sys.exit(main())

