# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Unit tests for the annotations module.
"""

import logging
import os
import shutil
import tempfile
import unittest

import numpy as np

from . import annotations
from . import input_signal_creator
from . import signal_processing


class TestAnnotationsExtraction(unittest.TestCase):
  """Unit tests for the annotations module.
  """

  _CLEAN_TMP_OUTPUT = False

  def setUp(self):
    """Create temporary folder."""
    self._tmp_path = tempfile.mkdtemp()
    self._wav_file_path = os.path.join(self._tmp_path, 'tone.wav')
    pure_tone, _ = input_signal_creator.InputSignalCreator.Create(
        'pure_tone', [440, 1000])
    signal_processing.SignalProcessingUtils.SaveWav(
        self._wav_file_path, pure_tone)

  def tearDown(self):
    """Recursively delete temporary folder."""
    if self._CLEAN_TMP_OUTPUT:
      shutil.rmtree(self._tmp_path)
    else:
      logging.warning(self.id() + ' did not clean the temporary path ' + (
          self._tmp_path))

  def testExtraction(self):
    e = annotations.AudioAnnotationsExtractor()
    e.Extract(self._wav_file_path)
    vad = e.GetVad()
    assert len(vad) > 0
    self.assertGreaterEqual(float(np.sum(vad)) / len(vad), 0.95)

  def testSaveLoad(self):
    e = annotations.AudioAnnotationsExtractor()
    e.Extract(self._wav_file_path)
    e.Save(self._tmp_path)
    np.testing.assert_array_equal(
        e.GetLevel(),
        np.load(os.path.join(self._tmp_path, e.GetLevelFileName())))
    np.testing.assert_array_equal(
        e.GetVad(),
        np.load(os.path.join(self._tmp_path, e.GetVadFileName())))
    np.testing.assert_array_equal(
        e.GetSpeechLevel(),
        np.load(os.path.join(self._tmp_path, e.GetSpeechLevelFileName())))
