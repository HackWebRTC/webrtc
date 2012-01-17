#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Implements the coverage tracker dashboard and reporting facilities."""

__author__ = 'phoglund@webrtc.org (Patrik Hoglund)'

import datetime
from google.appengine.ext import db
import webapp2
import gviz_api


class CoverageData(db.Model):
  """This represents one coverage report from the build bot."""
  date = db.DateTimeProperty(required=True)
  line_coverage = db.FloatProperty(required=True)
  function_coverage = db.FloatProperty(required=True)


class ShowDashboard(webapp2.RequestHandler):
  """Shows the dashboard page.

     The page is shown by grabbing data we have stored previously
     in the App Engine database using the AddCoverageData handler.
  """

  def get(self):
    page_template_filename = 'templates/dashboard_template.html'

    # Load the page HTML template.
    try:
      template_file = open(page_template_filename)
      page_template = template_file.read()
      template_file.close()
    except IOError as exception:
      self.ShowErrorPage('Cannot open page template file: %s<br>Details: %s' %
                         (page_template_filename, exception))
      return

    coverage_entries = db.GqlQuery('SELECT * '
                                   'FROM CoverageData '
                                   'ORDER BY date ASC')
    data = []
    for coverage_entry in coverage_entries:
      data.append({'date': coverage_entry.date,
                   'line_coverage': coverage_entry.line_coverage,
                   'function_coverage': coverage_entry.function_coverage,
                  })

    description = {
        'date': ('datetime', 'Date'),
        'line_coverage': ('number', 'Line Coverage'),
        'function_coverage': ('number', 'Function Coverage')
    }
    coverage_data = gviz_api.DataTable(description, data)
    coverage_json_data = coverage_data.ToJSon(order_by='date')

    # Fill in the template with the data and respond:
    self.response.write(page_template % vars())

  def ShowErrorPage(self, error_message):
    self.response.write('<html><body>%s</body></html>' % error_message)


class AddCoverageData(webapp2.RequestHandler):
  """Used to report coverage data.

     It will verify the data, but not the sender. Thus, it should be secured
     more properly if accessible from an outside network.
  """

  def get(self):
    try:
      posix_time = int(self.request.get('date'))
      parsed_date = datetime.datetime.fromtimestamp(posix_time)

      line_coverage = float(self.request.get('line_coverage'))
      function_coverage = float(self.request.get('function_coverage'))
    except ValueError as exception:
      self.ShowErrorPage('Invalid parameter in request. Details: %s' %
                         exception)
      return

    item = CoverageData(date=parsed_date,
                        line_coverage=line_coverage,
                        function_coverage=function_coverage)
    item.put()

  def ShowErrorPage(self, error_message):
    self.response.write('<html><body>%s</body></html>' % error_message)

app = webapp2.WSGIApplication([('/', ShowDashboard),
                               ('/add_coverage_data', AddCoverageData)],
                              debug=True)
