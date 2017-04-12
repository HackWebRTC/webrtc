#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import glob
import unittest
from video_analysis import FindUsbPortForV4lDevices


class RunVideoAnalysisTest(unittest.TestCase):
  def setGlobPath(self, path1, path2):
    self.path1 = path1
    self.path2 = path2

  def setUp(self):
    self.path1 = ''
    self.path2 = ''
    self.requestNbr = 1

    def glob_mock(string):
      # Eat incoming string.
      del string
      if self.requestNbr == 1:
        self.requestNbr += 1
        return self.path1
      else:
        self.requestNbr = 1
        return self.path2

    # Override the glob function with our own that returns a string set by the
    # test.
    glob.glob = glob_mock

  # Verifies that the correct USB id is returned.
  def testFindUSBPortForV4lDevices(self):
    short_path1 = ('/sys/bus/usb/devices/usb1/1-1/driver/4-4/4-4:1.0/'
                   'video4linux/video0')
    short_path2 = ('/sys/bus/usb/devices/usb1/1-1/driver/4-3/4-3:1.0/'
                   'video4linux/video1')
    self.setGlobPath(short_path1, short_path2)
    short_usb_ids = ['4-4', '4-3']
    self.assertEqual(FindUsbPortForV4lDevices('video0', 'video1'),
                     short_usb_ids)

    long_path1 = ('/sys/bus/usb/devices/usb1/1-1/driver/3-3/3-3.1:1.0/'
                  'video4linux/video0')
    long_path2 = ('/sys/bus/usb/devices/usb1/1-1/driver/3-2/3-2.1:1.0/'
                  'video4linux/video1')
    self.setGlobPath(long_path1, long_path2)
    long_usb_ids = ['3-3.1', '3-2.1']
    self.assertEqual(FindUsbPortForV4lDevices('video0', 'video1'), long_usb_ids)


  def testFindUSBPortForV4lDevicesNoDevice(self):
    noDeviceFound = ('')
    V4lDevice = ('/sys/bus/usb/devices/usb1/1-1/driver/3-2/3-2.1:1.0/'
                  'video4linux/video1')
    self.setGlobPath(noDeviceFound, V4lDevice)
    empty_list = []
    self.assertEqual(FindUsbPortForV4lDevices('video0', 'video1'), empty_list)


if __name__ == "__main__":
  unittest.main()
