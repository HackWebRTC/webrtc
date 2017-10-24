# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Unit tests for the annotations module.
"""

from __future__ import division
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

  _CLEAN_TMP_OUTPUT = True
  _DEBUG_PLOT_VAD = False

  def setUp(self):
    """Create temporary folder."""
    self._tmp_path = tempfile.mkdtemp()
    self._wav_file_path = os.path.join(self._tmp_path, 'tone.wav')
    pure_tone, _ = input_signal_creator.InputSignalCreator.Create(
        'pure_tone', [440, 1000])
    signal_processing.SignalProcessingUtils.SaveWav(
        self._wav_file_path, pure_tone)
    self._sample_rate = pure_tone.frame_rate

  def tearDown(self):
    """Recursively delete temporary folder."""
    if self._CLEAN_TMP_OUTPUT:
      shutil.rmtree(self._tmp_path)
    else:
      logging.warning(self.id() + ' did not clean the temporary path ' + (
          self._tmp_path))

  def testFrameSizes(self):
    for vad_type in annotations.AudioAnnotationsExtractor.VadType:
      e = annotations.AudioAnnotationsExtractor(vad_type=vad_type)
      e.Extract(self._wav_file_path)
      samples_to_ms = lambda n, sr: 1000 * n // sr
      self.assertEqual(samples_to_ms(e.GetLevelFrameSize(), self._sample_rate),
                       e.GetLevelFrameSizeMs())
      self.assertEqual(samples_to_ms(e.GetVadFrameSize(), self._sample_rate),
                       e.GetVadFrameSizeMs())

  def testVoiceActivityDetectors(self):
    for vad_type in annotations.AudioAnnotationsExtractor.VadType:
      e = annotations.AudioAnnotationsExtractor(vad_type=vad_type)
      e.Extract(self._wav_file_path)
      vad_output = e.GetVadOutput()
      self.assertGreater(len(vad_output), 0)
      self.assertGreaterEqual(float(np.sum(vad_output)) / len(vad_output), 0.95)

      if self._DEBUG_PLOT_VAD:
        frame_times_s = lambda num_frames, frame_size_ms: np.arange(
            num_frames).astype(np.float32) * frame_size_ms / 1000.0
        level = e.GetLevel()
        t_level = frame_times_s(
            num_frames=len(level),
            frame_size_ms=e.GetLevelFrameSizeMs())
        t_vad = frame_times_s(
            num_frames=len(vad_output),
            frame_size_ms=e.GetVadFrameSizeMs())
        import matplotlib.pyplot as plt
        plt.figure()
        plt.hold(True)
        plt.plot(t_level, level)
        plt.plot(t_vad, vad_output * np.max(level), '.')
        plt.show()

  def testSaveLoad(self):
    e = annotations.AudioAnnotationsExtractor(
        vad_type=annotations.AudioAnnotationsExtractor.VadType.ENERGY_THRESHOLD)
    e.Extract(self._wav_file_path)
    e.Save(self._tmp_path)

    data = np.load(os.path.join(self._tmp_path, e.GetOutputFileName()))
    np.testing.assert_array_equal(e.GetLevel(), data['level'])
    self.assertEqual(np.float32, data['level'].dtype)
    np.testing.assert_array_equal(e.GetVadOutput(), data['vad_output'])
    self.assertEqual(np.uint8, data['vad_output'].dtype)
