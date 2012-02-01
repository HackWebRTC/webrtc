#!/usr/bin/env python
#-*- coding: utf-8 -*-
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Contains functions for parsing the build master's transposed grid page."""

__author__ = 'phoglund@webrtc.org (Patrik Höglund)'

import unittest

import tgrid_parser


SAMPLE_FILE = """
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
 "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">

<html
 xmlns="http://www.w3.org/1999/xhtml"
 lang="en"
 xml:lang="en">
<head>
 <title>Buildbot</title>
 <link href="buildbot.css" rel="stylesheet" type="text/css" />
</head>

<body vlink="#800080">
<table class="Grid" border="0" cellspacing="0">
<tr>
<td valign="bottom" class="sourcestamp">1570</td>
<td class="build success">
  <a href="builders/Android/builds/121">OK</a></td>
<td class="build success">
  <a href="builders/ChromeOS/builds/578">OK</a></td>
<td class="build success">
  <a href="builders/Linux32bitDBG/builds/564">OK</a></td>
<td class="build success">
  <a href="builders/Linux32bitRelease/builds/684">OK</a></td>
<td class="build success">
  <a href="builders/Linux64bitDBG/builds/680">OK</a></td>
<td class="build success">
  <a href="builders/Linux64bitDBG-GCC4.6/builds/5">OK</a></td>
<td class="build success">
  <a href="builders/Linux64bitRelease/builds/570">OK</a></td>
<td class="build success">
  <a href="builders/LinuxCLANG/builds/259">OK</a></td>
<td class="build success">
  <a href="builders/LinuxVideoTest/builds/345">OK</a></td>
<td class="build success">
  <a href="builders/MacOS/builds/670">OK</a></td>
<td class="build success">
  <a href="builders/Win32Debug/builds/432">OK</a></td>
<td class="build success">
  <a href="builders/Win32Release/builds/440">OK</a></td>
</tr>
<tr>
<td valign="bottom" class="sourcestamp">1571</td>
<td class="build success">
  <a href="builders/Android/builds/122">OK</a></td>
<td class="build success">
  <a href="builders/ChromeOS/builds/579">OK</a></td>
<td class="build success">
  <a href="builders/Linux32bitDBG/builds/565">OK</a></td>
<td class="build success">
  <a href="builders/Linux32bitRelease/builds/685">OK</a></td>
<td class="build success">
  <a href="builders/Linux64bitDBG/builds/681">OK</a></td>
<td class="build success">
  <a href="builders/Linux64bitDBG-GCC4.6/builds/6">OK</a></td>
<td class="build success">
  <a href="builders/Linux64bitRelease/builds/571">OK</a></td>
<td class="build success">
  <a href="builders/LinuxCLANG/builds/260">OK</a></td>
<td class="build failure">
  <a href="builders/LinuxVideoTest/builds/346">failed</a><br />
voe_auto_test</td>
<td class="build success">
  <a href="builders/MacOS/builds/671">OK</a></td>
<td class="build running">
  <a href="builders/Win32Debug/builds/441">building</a></td>
<td class="build success">
  <a href="builders/Win32Release/builds/441">OK</a></td>
</tr>
</table>
</body>
</html>
"""

MINIMAL_OK = """
<tr>
<td valign="bottom" class="sourcestamp">1570</td>
<td class="build success">
<a href="builders/Android/builds/121">OK</a></td>
</tr>
"""

MINIMAL_FAIL = """
<tr>
<td valign="bottom" class="sourcestamp">1573</td>
<td class="build failure">
<a href="builders/LinuxVideoTest/builds/347">failed</a><br />
voe_auto_test</td>
</tr>
"""

MINIMAL_BUILDING = """
<tr>
<td valign="bottom" class="sourcestamp">1576</td>
<td class="build running">
<a href="builders/Win32Debug/builds/434">building</a></td>
voe_auto_test</td>
</tr>
"""

class TGridParserTest(unittest.TestCase):
  def test_parser_throws_exception_on_empty_html(self):
    self.assertRaises(tgrid_parser.FailedToParseBuildStatus,
                      tgrid_parser.parse_tgrid_page, '');

  def test_parser_finds_successful_bot(self):
    result = tgrid_parser.parse_tgrid_page(MINIMAL_OK)

    self.assertEqual(1, len(result), 'There is only one bot in the sample.')
    first_mapping = result.items()[0]

    self.assertEqual('1570--Android', first_mapping[0])
    self.assertEqual('121--OK', first_mapping[1])

  def test_parser_finds_failed_bot(self):
    result = tgrid_parser.parse_tgrid_page(MINIMAL_FAIL)

    self.assertEqual(1, len(result), 'There is only one bot in the sample.')
    first_mapping = result.items()[0]

    self.assertEqual('1573--LinuxVideoTest', first_mapping[0])
    self.assertEqual('347--failed', first_mapping[1])

  def test_parser_finds_building_bot(self):
    result = tgrid_parser.parse_tgrid_page(MINIMAL_BUILDING)

    self.assertEqual(1, len(result), 'There is only one bot in the sample.')
    first_mapping = result.items()[0]

    self.assertEqual('1576--Win32Debug', first_mapping[0])
    self.assertEqual('434--building', first_mapping[1])

  def test_parser_finds_all_bots_and_revisions(self):
    result = tgrid_parser.parse_tgrid_page(SAMPLE_FILE)

    # 2 * 12 = 24 bots in sample
    self.assertEqual(24, len(result))

    # Make some samples
    self.assertTrue(result.has_key('1570--ChromeOS'))
    self.assertEquals('578--OK', result['1570--ChromeOS'])

    self.assertTrue(result.has_key('1570--LinuxCLANG'))
    self.assertEquals('259--OK', result['1570--LinuxCLANG'])

    self.assertTrue(result.has_key('1570--Win32Release'))
    self.assertEquals('440--OK', result['1570--Win32Release'])

    self.assertTrue(result.has_key('1571--ChromeOS'))
    self.assertEquals('579--OK', result['1571--ChromeOS'])

    self.assertTrue(result.has_key('1571--LinuxVideoTest'))
    self.assertEquals('346--failed', result['1571--LinuxVideoTest'])

    self.assertTrue(result.has_key('1571--Win32Debug'))
    self.assertEquals('441--building', result['1571--Win32Debug'])


if __name__ == '__main__':
  unittest.main()
