#!/usr/bin/env python
# Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Upload data to the chrome perf dashboard via add_histograms endpoint."""

import os
import sys
import logging

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
CHECKOUT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, os.pardir, os.pardir))
sys.path.insert(0, os.path.join(CHECKOUT_ROOT, 'third_party', 'catapult',
                                'tracing'))

from tracing.value import histogram
from tracing.value import histogram_set
from tracing.value.diagnostics import generic_set
from tracing.value.diagnostics import reserved_infos

# Enums aren't supported in Chromium's python env, so do something similar:
class ImprovementDirection(object):
  DEFAULT = 1
  BIGGER_IS_BETTER = 2
  SMALLER_IS_BETTER = 3


def MakeWebRtcHistogramSet(stats, commit_pos, commit_hash, master, bot,
                           test_suite, build_url):
  """Converts a dict of stats into a list of points with additional info.

  Args:
    stats: A list of histograms to upload.
    piper_revision: Baseline piper revision that the test was run on.
    commit_hash: WebRTC commit hash that the test was run on.
    master:
    bot: Bot name as it will show up in the perf dashboard.
    test_suite: Top-level identifier of the test for Chrome perf dashboard.
    build_url: An URL pointing to the bot status page for this build.

  Returns:
    A histogram set in format that expect Chrome perf dashboard.
  """
  common_diagnostics = {
      reserved_infos.MASTERS: master,
      reserved_infos.BOTS: bot,
      reserved_infos.POINT_ID: commit_pos,
      reserved_infos.BENCHMARKS: test_suite,
      reserved_infos.WEBRTC_REVISIONS: str(commit_hash),
      reserved_infos.BUILD_URLS: build_url,
  }

  hs = histogram_set.HistogramSet()
  for h in stats:
    hs.AddHistogram(h)

  for k, v in common_diagnostics.items():
    hs.AddSharedDiagnosticToAllHistograms(k.name, generic_set.GenericSet([v]))

  return hs


def LoadHistograms(data):
  """Load histograms from Chart JSON format json file and fix them for API.

  Args:
    data: parsed json object of Chart JSON format.

  Raises:
    RuntimeError: input data contains standard deviation section.
  Returns:
    list of loaded histograms.
  """
  stats = []
  for metric, story in data['charts'].items():
    for story_name, story_desc in story.items():
      units = story_desc['units'].strip()
      if 'std' in story_desc:
        # TODO(bugs.webrtc.org/11084): This seems bad to throw away?
        logging.debug('std is not supported, specify list of values instead.')

      if 'value' in story_desc:
        values = [story_desc['value']]
      else:
        values = list(story_desc['values'])

      improvement_direction = ImprovementDirection.DEFAULT
      if 'improvement_direction' in story_desc:
        if story_desc['improvement_direction'] == 'bigger_is_better':
          improvement_direction = ImprovementDirection.BIGGER_IS_BETTER
        elif story_desc['improvement_direction'] == 'smaller_is_better':
          improvement_direction = ImprovementDirection.SMALLER_IS_BETTER
      if 'higher_is_better' in story_desc:
        if story_desc['higher_is_better']:
          improvement_direction = ImprovementDirection.BIGGER_IS_BETTER
        else:
          improvement_direction = ImprovementDirection.SMALLER_IS_BETTER

      new_metric, new_units, new_values = _FixUnits(metric, units, values)
      h = _BuildHistogram(new_metric, story_name, new_units, new_values,
                          improvement_direction)
      stats.append(h)
  return stats


def _FixUnits(metric_name, units, values):
  """Fix units and metric name with values if required.

  Args:
    metric_name: origin metric name
    units: raw trimmed units
    values: origin values

  Returns:
    (metric_name, units, values) triple with fixed content
  """
  if units == 'bps':
    return metric_name, 'bytesPerSecond', [v / 8.0 for v in values]
  elif units == 'dB':
    return metric_name + '_dB', 'unitless', values
  elif units == 'fps':
    return metric_name + '_fps', 'Hz', values
  elif units == 'frames':
    return metric_name, 'count', values
  elif units == 'ms':
    return metric_name, 'msBestFitFormat', values
  elif units == '%':
    return metric_name + '_%', 'unitless', values
  else:
    return metric_name, units, values


def _BuildHistogram(metric_name, story_name, units, values,
                    improvement_direction):
  """Build histogram. Uses unitless for unsupported units."""
  if units not in histogram.UNIT_NAMES:
    logging.debug(
        'Unsupported unit %s will be replaced by \'unitless\'', units)
    units = 'unitless'
  if improvement_direction is ImprovementDirection.BIGGER_IS_BETTER:
    units = units + '_biggerIsBetter'
  elif improvement_direction is ImprovementDirection.SMALLER_IS_BETTER:
    units = units + '_smallerIsBetter'
  h = histogram.Histogram(metric_name, units,
                          histogram.HistogramBinBoundaries.SINGULAR)
  h.diagnostics[reserved_infos.STORIES.name] = generic_set.GenericSet(
      [story_name])
  h.CustomizeSummaryOptions({
      'std': False,
      'avg': False,
      'count': False,
      'max': False,
      'min': False,
      'sum': False
  })
  for v in values:
    h.AddSample(v)
  return h
