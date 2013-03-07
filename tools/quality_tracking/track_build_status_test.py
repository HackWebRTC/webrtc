#!/usr/bin/env python
#-*- coding: utf-8 -*-
# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Unit test for the build status tracker script."""


import copy
import unittest

import track_build_status


NORMAL_BOT_TO_STATUS_MAPPING = {
    '1455--Win 64 Release': '455--OK',
    '1455--CrOS': '900--failed',
    '1455--Linux32 Debug': '344--OK',
    '1456--Win Large Tests': '456--OK'}


class TrackBuildStatusTest(unittest.TestCase):

  def test_get_desired_bots(self):
    bot_to_status_mapping = copy.deepcopy(NORMAL_BOT_TO_STATUS_MAPPING)
    desired_bot_names = ['Linux32 Debug']
    # pylint: disable=W0212
    result = track_build_status._filter_undesired_bots(bot_to_status_mapping,
                                                       desired_bot_names)
    self.assertEquals(1, len(result))
    self.assertTrue(desired_bot_names[0] in result.keys()[0])

if __name__ == '__main__':
  unittest.main()
