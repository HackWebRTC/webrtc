#!/usr/bin/env python
#-*- coding: utf-8 -*-
# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""This script grabs and reports coverage information.

   It grabs coverage information from the latest Linux 32-bit build and
   pushes it to the coverage tracker, enabling us to track code coverage
   over time. This script is intended to run on the 32-bit Linux slave.
"""

__author__ = 'phoglund@webrtc.org (Patrik Höglund)'

import httplib
import os
import re
import sys
import time

# The build-bot user which runs build bot jobs.
BUILD_BOT_USER = 'phoglund'

# The server to send coverage data to.
# TODO(phoglund): replace with real server once we get it up.
DASHBOARD_SERVER = 'localhost:8080'


class FailedToParseCoverageHtml(Exception):
  pass


class FailedToReportToDashboard(Exception):
  pass


def _find_latest_32bit_debug_build(www_directory_contents):
  # Build directories have the form Linux32bitDebug_<number>. There may be other
  # directories in the list though, for instance for other build configurations.
  # This sort ensures we will encounter the directory with the highest number
  # first.
  www_directory_contents.sort(reverse=True)

  for entry in www_directory_contents:
    match = re.match('Linux32bitDBG_\d+', entry)
    if match is not None:
      return entry

  # Didn't find it
  return None


def _grab_coverage_percentage(label, index_html_contents):
  """Extracts coverage from a LCOV coverage report.

     Grabs coverage by assuming that the label in the coverage HTML report
     is close to the actual number and that the number is followed by a space
     and a percentage sign.
  """
  match = re.search('<td[^>]*>' + label + '</td>.*?(\d+\.\d) %',
                    index_html_contents, re.DOTALL)
  if match is None:
    raise FailedToParseCoverageHtml('Missing coverage at label "%s".' % label)

  try:
    return float(match.group(1))
  except ValueError:
    raise FailedToParseCoverageHtml('%s is not a float.' % match.group(1))


def _report_coverage_to_dashboard(now, line_coverage, function_coverage):
  request_string = ('/add_coverage_data?'
                    'date=%d&line_coverage=%f&function_coverage=%f' %
                    (now, line_coverage, function_coverage))

  connection = httplib.HTTPConnection(DASHBOARD_SERVER)
  connection.request('GET', request_string)
  response = connection.getresponse()
  if response.status != 200:
    message = ('Error: Failed to report to %s%s: got response %d (%s)' %
               (DASHBOARD_SERVER, request_string, response.status,
                response.reason))
    raise FailedToReportToDashboard(message)

  # The response content should be empty on success, so check that:
  response_content = response.read()
  if response_content:
    message = ('Error: Dashboard reported the following error: %s.' %
               response_content)
    raise FailedToReportToDashboard(message)


def _main():
  coverage_www_dir = os.path.join('/home', BUILD_BOT_USER, 'www')

  www_dir_contents = os.listdir(coverage_www_dir)
  latest_build_directory = _find_latest_32bit_debug_build(www_dir_contents)

  if latest_build_directory is None:
    print 'Error: Found no 32-bit debug build in directory ' + coverage_www_dir
    sys.exit(1)

  index_html_path = os.path.join(coverage_www_dir, latest_build_directory,
                                 'index.html')
  index_html_file = open(index_html_path)
  whole_file = index_html_file.read()

  line_coverage = _grab_coverage_percentage('Lines:', whole_file)
  function_coverage = _grab_coverage_percentage('Functions:', whole_file)
  now = int(time.time())

  _report_coverage_to_dashboard(now, line_coverage, function_coverage)

if __name__ == '__main__':
  _main()

