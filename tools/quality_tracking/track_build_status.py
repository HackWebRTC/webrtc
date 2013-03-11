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

import httplib

import constants
import dashboard_connection
import tgrid_parser

# Bots that must be green in order to increment the LKGR revision.
BOTS = ['Win32 Debug',
        'Win32 Release',
        'Mac32 Debug',
        'Mac32 Release',
        'Mac64 Debug',
        'Mac64 Release',
        'Linux32 Debug',
        'Linux32 Release',
        'Linux64 Debug',
        'Linux64 Release',
        'Linux Clang',
        'Linux64 Debug GCC4.6',
        'Linux Memcheck',
        'Linux Tsan',
        'Linux Asan',
        'Win Large Tests',
        'Mac Large Tests',
        'Linux Large Tests',
        'CrOS',
        'Android',
        'Android NDK',
       ]


class FailedToGetStatusFromMaster(Exception):
  pass


def _download_and_parse_build_status():
  connection = httplib.HTTPConnection(constants.BUILD_MASTER_SERVER)
  connection.request('GET', constants.BUILD_MASTER_TRANSPOSED_GRID_URL)
  response = connection.getresponse()

  if response.status != 200:
    raise FailedToGetStatusFromMaster(('Failed to get build status from master:'
                                       ' got status %d, reason %s.' %
                                       (response.status, response.reason)))

  full_response = response.read()
  connection.close()

  return tgrid_parser.parse_tgrid_page(full_response)


def _is_chrome_only_build(revision_to_bot_name):
  """Figures out if a revision-to-bot-name mapping represents a Chrome build.

  We assume here that Chrome revisions are always > 100000, whereas WebRTC
  revisions will not reach that number in the foreseeable future.
  """
  revision = int(revision_to_bot_name.split('--')[0])
  bot_name = revision_to_bot_name.split('--')[1]
  return 'Chrome' in bot_name and revision > 100000


def _filter_undesired_bots(bot_to_status_mapping, desired_bot_names):
  """Returns the desired bots for the builds status from the dictionary.

  Args:
    bot_to_status_mapping: Dictionary mapping bot name with revision to status.
    desired_bot_names: List of bot names that will be the only bots returned in
      the resulting dictionary.
  Returns: A dictionary only containing the desired bots.
  """
  result = {}
  for revision_to_bot_name, status in bot_to_status_mapping.iteritems():
    bot_name = revision_to_bot_name.split('--')[1]
    if bot_name in desired_bot_names:
      result[revision_to_bot_name] = status
  return result


def _main():
  dashboard = dashboard_connection.DashboardConnection(constants.CONSUMER_KEY)
  dashboard.read_required_files(constants.CONSUMER_SECRET_FILE,
                                constants.ACCESS_TOKEN_FILE)

  bot_to_status_mapping = _download_and_parse_build_status()
  bot_to_status_mapping = _filter_undesired_bots(bot_to_status_mapping, BOTS)

  dashboard.send_post_request(constants.ADD_BUILD_STATUS_DATA_URL,
                              bot_to_status_mapping)


if __name__ == '__main__':
  _main()
