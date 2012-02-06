#!/usr/bin/env python
#-*- coding: utf-8 -*-
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Loads coverage data from the database."""

__author__ = 'phoglund@webrtc.org (Patrik Höglund)'

from google.appengine.ext import db
import gviz_api


class CoverageDataLoader:
  """ Loads coverage data from the database."""

  def load_coverage_json_data(self):
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
    return coverage_data.ToJSon(order_by='date')
