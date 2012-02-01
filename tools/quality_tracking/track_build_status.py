#!/usr/bin/env python
#-*- coding: utf-8 -*-
# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""This script checks the current build status on the master and submits
   it to the dashboard. It is adapted to build bot version 0.7.12.
"""

__author__ = 'phoglund@webrtc.org (Patrik Höglund)'


import httplib
import re

import constants
import dashboard_connection


class FailedToGetStatusFromMaster(Exception):
  pass


class FailedToParseBuildStatus(Exception):
  pass


def _parse_status_page(html):
  """Parses the build master's one_box_per_builder page.

     Args:
         html: The raw HTML from the one_box_per_builder page.

     Returns: the bot name mapped to a string with the build number and the
         build status separated by a dash (e.g. 456-successful, 114-failed).
  """
  result = {}

  # Example target string: <a href="builders/Win32Debug/builds/430">#430</a>
  #                        <br />build<br />successful</td>
  # Group 1 captures 'Win32Debug', Group 2 captures '430', group 3 'successful'.
  # Implementation note: We match non-greedily (.*?) between the link and
  # successful / failed, otherwise we would only find the first status.
  for match in re.finditer('<a href="builders/([^/]*)/builds/([0-9]+)">'
                           '.*?(successful|failed)',
                           html, re.DOTALL):
    result[match.group(1)] = match.group(2) + '-' + match.group(3)

  if not result:
    raise FailedToParseBuildStatus('Could not find any build statuses in %s.' %
                                   html)

  return result


def _download_and_parse_build_status():
  connection = httplib.HTTPConnection(constants.BUILD_MASTER_SERVER)
  connection.request('GET', constants.BUILD_MASTER_LATEST_BUILD_URL)
  response = connection.getresponse()

  if response.status != 200:
    raise FailedToGetStatusFromMaster(('Failed to get build status from master:'
                                       ' got status %d, reason %s.' %
                                       (response.status, response.reason)))

  full_response = response.read()
  connection.close()

  return _parse_status_page(full_response)


def _main():
  dashboard = dashboard_connection.DashboardConnection(constants.CONSUMER_KEY)
  dashboard.read_required_files(constants.CONSUMER_SECRET_FILE,
                                constants.ACCESS_TOKEN_FILE)

  bot_to_status_mapping = _download_and_parse_build_status()

  response = dashboard.send_post_request(constants.ADD_BUILD_STATUS_DATA_URL,
                                         bot_to_status_mapping)

  print response.read()

if __name__ == '__main__':
  _main()
