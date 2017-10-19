# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Extraction of annotations from audio files.
"""

from __future__ import division
import logging
import os
import sys

try:
  import numpy as np
except ImportError:
  logging.critical('Cannot import the third-party Python package numpy')
  sys.exit(1)

from . import signal_processing


class AudioAnnotationsExtractor(object):
  """Extracts annotations from audio files.
  """

  _LEVEL_FILENAME = 'level.npy'
  _VAD_FILENAME = 'vad.npy'
  _SPEECH_LEVEL_FILENAME = 'speech_level.npy'

  # Level estimation params.
  _ONE_DB_REDUCTION = np.power(10.0, -1.0 / 20.0)
  _LEVEL_FRAME_SIZE_MS = 1.0
  # The time constants in ms indicate the time it takes for the level estimate
  # to go down/up by 1 db if the signal is zero.
  _LEVEL_ATTACK_MS = 5.0
  _LEVEL_DECAY_MS = 20.0

  # VAD params.
  _VAD_THRESHOLD = 1

  def __init__(self):
    self._signal = None
    self._level = None
    self._vad = None
    self._speech_level = None
    self._level_frame_size = None
    self._c_attack = None
    self._c_decay = None

  @classmethod
  def GetLevelFileName(cls):
    return cls._LEVEL_FILENAME

  @classmethod
  def GetVadFileName(cls):
    return cls._VAD_FILENAME

  @classmethod
  def GetSpeechLevelFileName(cls):
    return cls._SPEECH_LEVEL_FILENAME

  def GetLevel(self):
    return self._level

  def GetVad(self):
    return self._vad

  def GetSpeechLevel(self):
    return self._speech_level

  def Extract(self, filepath):
    # Load signal.
    self._signal = signal_processing.SignalProcessingUtils.LoadWav(filepath)
    if self._signal.channels != 1:
      raise NotImplementedError('multiple-channel annotations not implemented')

    # level estimation params.
    self._level_frame_size = int(self._signal.frame_rate / 1000 * (
        self._LEVEL_FRAME_SIZE_MS))
    self._c_attack = 0.0 if self._LEVEL_ATTACK_MS == 0 else (
        self._ONE_DB_REDUCTION ** (
            self._LEVEL_FRAME_SIZE_MS / self._LEVEL_ATTACK_MS))
    self._c_decay = 0.0 if self._LEVEL_DECAY_MS == 0 else (
        self._ONE_DB_REDUCTION ** (
            self._LEVEL_FRAME_SIZE_MS / self._LEVEL_DECAY_MS))

    # Compute level.
    self._LevelEstimation()

    # Naive VAD based on level thresholding. It assumes ideal clean speech
    # with high SNR.
    # TODO(alessiob): Maybe replace with a VAD based on stationary-noise
    # detection.
    vad_threshold = np.percentile(self._level, self._VAD_THRESHOLD)
    self._vad = np.uint8(self._level > vad_threshold)

    # Speech level based on VAD output.
    self._speech_level = self._level * self._vad

    # Expand to one value per sample.
    self._level = np.repeat(self._level, self._level_frame_size)
    self._vad = np.repeat(self._vad, self._level_frame_size)
    self._speech_level = np.repeat(self._speech_level, self._level_frame_size)

  def Save(self, output_path):
    np.save(os.path.join(output_path, self._LEVEL_FILENAME), self._level)
    np.save(os.path.join(output_path, self._VAD_FILENAME), self._vad)
    np.save(os.path.join(output_path, self._SPEECH_LEVEL_FILENAME),
            self._speech_level)

  def _LevelEstimation(self):
    # Read samples.
    samples = signal_processing.SignalProcessingUtils.AudioSegmentToRawData(
        self._signal).astype(np.float32) / 32768.0
    num_frames = len(samples) // self._level_frame_size
    num_samples = num_frames * self._level_frame_size

    # Envelope.
    self._level = np.max(np.reshape(np.abs(samples[:num_samples]), (
        num_frames, self._level_frame_size)), axis=1)
    assert len(self._level) == num_frames

    # Envelope smoothing.
    smooth = lambda curr, prev, k: (1 - k) * curr  + k * prev
    self._level[0] = smooth(self._level[0], 0.0, self._c_attack)
    for i in range(1, num_frames):
      self._level[i] = smooth(
          self._level[i], self._level[i - 1], self._c_attack if (
              self._level[i] > self._level[i - 1]) else self._c_decay)

    return self._level
