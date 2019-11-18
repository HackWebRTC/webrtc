#!/usr/bin/env python
# Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
CHECKOUT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, os.pardir, os.pardir))
sys.path.insert(0, os.path.join(CHECKOUT_ROOT, 'third_party', 'catapult',
                                'tracing'))
sys.path.append(os.path.join(CHECKOUT_ROOT, 'third_party', 'pymock'))

import json
import mock
import unittest

import histogram_util as u

from tracing.value import histogram
from tracing.value.diagnostics import generic_set
from tracing.value.diagnostics import reserved_infos


class HistogramUploaderUnittest(unittest.TestCase):

  def testLoadHistogramsWithValues(self):
    data = json.loads("""
    {
      "format_version": "1.0",
      "charts": {
        "audio_score": {
          "AV": {
            "type": "scalar",
            "values": [0.6, 0.5, 0.7],
            "units": "unitless_biggerIsBetter"
          }
        }
      }
    }
    """)
    stats = u.LoadHistograms(data)
    self.assertEqual(len(stats), 1)
    self.assertEqual(stats[0].name, "audio_score")
    self.assertEqual(stats[0].unit, "unitless_biggerIsBetter")
    self.assertEqual(stats[0].sample_values, [0.6, 0.5, 0.7])

  def testLoadHistogramsWithValue(self):
    data = json.loads("""
    {
      "format_version": "1.0",
      "charts": {
        "audio_score": {
          "AV": {
            "type": "scalar",
            "value": 0.3,
            "units": "unitless_biggerIsBetter"
          }
        }
      }
    }
    """)
    stats = u.LoadHistograms(data)
    self.assertEqual(len(stats), 1)
    self.assertEqual(stats[0].name, "audio_score")
    self.assertEqual(stats[0].unit, "unitless_biggerIsBetter")
    self.assertEqual(stats[0].sample_values, [0.3])

  def testLoadHistogramsWithUnknownUnit(self):
    data = json.loads("""
    {
      "format_version": "1.0",
      "charts": {
        "audio_score": {
          "AV": {
            "type": "scalar",
            "value": 0.3,
            "units": "good_score_biggerIsBetter"
          }
        }
      }
    }
    """)
    stats = u.LoadHistograms(data)
    self.assertEqual(len(stats), 1)
    self.assertEqual(stats[0].name, "audio_score")
    self.assertEqual(stats[0].unit, "unitless")
    self.assertEqual(stats[0].sample_values, [0.3])

  def testLoadHistogramsWithStd(self):
    data = json.loads("""
    {
      "format_version": "1.0",
      "charts": {
        "audio_score": {
          "AV": {
            "type": "scalar",
            "value": 0.3,
            "std": 0.1,
            "units": "unitless",
            "higher_is_better": true
          }
        }
      }
    }
    """)
    stats = u.LoadHistograms(data)
    self.assertEqual(len(stats), 1)
    self.assertEqual(stats[0].name, "audio_score")
    self.assertEqual(stats[0].unit, "unitless_biggerIsBetter")
    self.assertEqual(stats[0].sample_values, [0.3])

  def testLoadHistogramsMsBiggerIsBetter(self):
    data = json.loads("""
    {
      "format_version": "1.0",
      "charts": {
        "audio_score": {
          "AV": {
            "type": "scalar",
            "value": 0.3,
            "std": 0.1,
            "units": "ms",
            "improvement_direction": "bigger_is_better"
          }
        }
      }
    }
    """)
    stats = u.LoadHistograms(data)
    self.assertEqual(len(stats), 1)
    self.assertEqual(stats[0].name, "audio_score")
    self.assertEqual(stats[0].unit, "msBestFitFormat_biggerIsBetter")
    self.assertEqual(stats[0].sample_values, [0.3])

  def testLoadHistogramsBps(self):
    data = json.loads("""
    {
      "format_version": "1.0",
      "charts": {
        "audio_score": {
          "AV": {
            "type": "scalar",
            "values": [240, 160],
            "std": 0.1,
            "units": "bps"
          }
        }
      }
    }
    """)
    stats = u.LoadHistograms(data)
    self.assertEqual(len(stats), 1)
    self.assertEqual(stats[0].name, "audio_score")
    self.assertEqual(stats[0].unit, "bytesPerSecond")
    self.assertEqual(stats[0].sample_values, [30, 20])

  def testMakeWebRtcHistogramSet(self):
    h = histogram.Histogram("audio_score", "unitless_biggerIsBetter",
                            histogram.HistogramBinBoundaries.SINGULAR)
    h.AddSample(0.5)
    h.diagnostics[reserved_infos.STORIES.name] = generic_set.GenericSet(["AV"])
    h.CustomizeSummaryOptions({
        "std": False,
        "avg": False,
        "count": False,
        "max": False,
        "min": False,
        "sum": False
    })
    stats = [h]
    build_url = ('https://ci.chromium.org/p/webrtc/builders/ci/'
                 'Android64%20%28M%20Nexus5X%29%28dbg%29')
    hs = u.MakeWebRtcHistogramSet(
        stats, commit_pos=123456789,
        commit_hash="da39a3ee5e6b4b0d3255bfef95601890afd80709",
        master="master", bot="bot", test_suite="webrtc_test_suite",
        build_url=build_url)

    expected = [{
        "guid": mock.ANY,
        "type": "GenericSet",
        "values": [123456789]
    }, {
        "guid": mock.ANY,
        "type": "GenericSet",
        "values": ["webrtc_test_suite"]
    }, {
        "guid": mock.ANY,
        "type": "GenericSet",
        "values": ["bot"]
    }, {
        "guid": mock.ANY,
        "type": "GenericSet",
        "values": ["master"]
    }, {
        "guid": mock.ANY,
        "type": "GenericSet",
        "values": ["da39a3ee5e6b4b0d3255bfef95601890afd80709"]
    }, {
        "guid": mock.ANY,
        "type": "GenericSet",
        "values": [build_url]
    }, {
        "sampleValues": [0.5],
        "name": "audio_score",
        "running": [1, 0.5, -0.6931471805599453, 0.5, 0.5, 0.5, 0],
        "diagnostics": {
            "benchmarks": mock.ANY,
            "bots": mock.ANY,
            "buildUrls": mock.ANY,
            "pointId": mock.ANY,
            "masters": mock.ANY,
            "stories": {
                "type": "GenericSet",
                "values": ["AV"]
            },
            "webrtcRevisions": mock.ANY
        },
        "allBins": [[1]],
        "summaryOptions": {
            "avg": False,
            "count": False,
            "max": False,
            "min": False,
            "std": False,
            "sum": False
        },
        "unit": "unitless_biggerIsBetter"
    }]
    self.maxDiff = None  # pylint: disable=C0103
    self.assertItemsEqual(expected, hs.AsDicts())


if __name__ == "__main__":
  unittest.main()
