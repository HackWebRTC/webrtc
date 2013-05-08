#!/usr/bin/env python
#  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Checks if a virtual webcam is running and starts it if not.

Returns a non-zero return code if the webcam could not be started.

Prerequisites:
* Python must have the psutil package installed.
* Windows: a scheduled task named 'ManyCam' must exist and be configured to
  launch ManyCam preconfigured to auto-play the test clip.
* Mac: ManyCam must be installed in the default location and be preconfigured
  to auto-play the test clip.
* Linux: The v4l2loopback must be loaded to the kernel already (with the
  devices=2 argument) and the v4l2_file_player application must be compiled and
  put in the location specified below.
"""

import psutil
import subprocess
import sys


WEBCAM_WIN = ['schtasks', '/run', '/tn', 'ManyCam']
WEBCAM_MAC = ['open', '/Applications/ManyCam/ManyCam.app']
WEBCAM_LINUX = (
    '$HOME/fake-webcam-driver/linux/v4l2_file_player/v4l2_file_player '
    '$HOME/webrtc_video_quality/reference_video.yuv 640 480 /dev/video1 &')


def IsWebCamRunning():
  if sys.platform == 'win32':
    process_name = 'ManyCam.exe'
  elif sys.platform.startswith('darwin'):
    process_name = 'ManyCam'
  elif sys.platform.startswith('linux'):
    process_name = 'v4l2_file_player'
  else:
    raise Exception('Unsupported platform: %s' % sys.platform)
  for p in psutil.get_process_list():
    if process_name == p.name:
      print 'Found a running virtual webcam (%s with PID %s)' % (p.name, p.pid)
      return True
  return False


def Main():
  if IsWebCamRunning():
    return 0

  try:
    if sys.platform == 'win32':
      subprocess.check_call(WEBCAM_WIN)
    elif sys.platform.startswith('darwin'):
      subprocess.check_call(WEBCAM_MAC)
    elif sys.platform.startswith('linux'):
      subprocess.check_call(WEBCAM_LINUX, shell=True)

    print 'Successfully launched virtual webcam.'
    return 0

  except Exception as e:
    print 'Failed to launch virtual webcam: %s' % e


if __name__ == '__main__':
  sys.exit(Main())
