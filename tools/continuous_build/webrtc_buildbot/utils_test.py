#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

__author__ = 'kjellander@webrtc.org (Henrik Kjellander)'

import unittest

from webrtc_buildbot import utils

class Test(unittest.TestCase):

  def testGetEnabledTests(self):
    tests = {
             # Test name                        Linux Mac    Windows
             "test_1":                         (True, True,  False),
             "test_2":                         (True, False, False),
    }
    result = utils.GetEnabledTests(tests, "Linux")
    self.assertEqual(2, len(result))
    self.assertEqual('test_1', result[0])
    self.assertEqual('test_2', result[1])

    result = utils.GetEnabledTests(tests, "Mac")
    self.assertEqual(1, len(result))
    self.assertEqual('test_1', result[0])

    result = utils.GetEnabledTests(tests, "Windows")
    self.assertEqual(0, len(result))

    self.assertRaises(utils.UnsupportedPlatformError,
                      utils.GetEnabledTests, tests, "BeOS")

if __name__ == "__main__":
  unittest.main()
