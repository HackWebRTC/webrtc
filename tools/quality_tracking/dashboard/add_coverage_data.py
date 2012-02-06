#!/usr/bin/env python
#-*- coding: utf-8 -*-
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Implements a handler for adding coverage data."""

__author__ = 'phoglund@webrtc.org (Patrik Höglund)'

import datetime

from google.appengine.ext import db

import oauth_post_request_handler

class CoverageData(db.Model):
  """This represents one coverage report from the build bot."""
  date = db.DateTimeProperty(required=True)
  line_coverage = db.FloatProperty(required=True)
  function_coverage = db.FloatProperty(required=True)


def _parse_percentage(string_value):
  percentage = float(string_value)
  if percentage < 0.0 or percentage > 100.0:
    raise ValueError('%s is not a valid percentage.' % string_value)
  return percentage


class AddCoverageData(oauth_post_request_handler.OAuthPostRequestHandler):
  """Used to report coverage data.

     Coverage data is reported as a POST request and should contain, aside from
     the regular oauth_* parameters, these values:

     date: The POSIX timestamp for when the coverage observation was made.
     line_coverage: A float percentage in the interval 0-100.0.
     function_coverage: A float percentage in the interval 0-100.0.
  """

  def _parse_and_store_data(self):
    try:
      posix_time = int(self.request.get('date'))
      parsed_date = datetime.datetime.fromtimestamp(posix_time)

      line_coverage_string = self.request.get('line_coverage')
      line_coverage = _parse_percentage(line_coverage_string)
      function_coverage_string = self.request.get('function_coverage')
      function_coverage = _parse_percentage(function_coverage_string)

    except ValueError as exception:
      self._show_error_page('Invalid parameter in request. Details: %s' %
                            exception)
      return

    item = CoverageData(date=parsed_date,
                        line_coverage=line_coverage,
                        function_coverage=function_coverage)
    item.put()

