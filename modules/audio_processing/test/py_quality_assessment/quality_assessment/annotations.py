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
import enum
import logging
import os
import shutil
import struct
import subprocess
import sys
import tempfile

try:
  import numpy as np
except ImportError:
  logging.critical('Cannot import the third-party Python package numpy')
  sys.exit(1)

from . import exceptions
from . import signal_processing


class AudioAnnotationsExtractor(object):
  """Extracts annotations from audio files.
  """

  @enum.unique
  class VadType(enum.Enum):
    ENERGY_THRESHOLD = 0  # TODO(alessiob): Consider switching to P56 standard.
    WEBRTC = 1

  _OUTPUT_FILENAME = 'annotations.npz'

  # Level estimation params.
  _ONE_DB_REDUCTION = np.power(10.0, -1.0 / 20.0)
  _LEVEL_FRAME_SIZE_MS = 1.0
  # The time constants in ms indicate the time it takes for the level estimate
  # to go down/up by 1 db if the signal is zero.
  _LEVEL_ATTACK_MS = 5.0
  _LEVEL_DECAY_MS = 20.0

  # VAD params.
  _VAD_THRESHOLD = 1
  _VAD_WEBRTC_PATH = os.path.join(os.path.dirname(
      os.path.abspath(__file__)), os.pardir, os.pardir)
  _VAD_WEBRTC_BIN_PATH = os.path.join(_VAD_WEBRTC_PATH, 'vad')

  def __init__(self, vad_type):
    self._signal = None
    self._level = None
    self._level_frame_size = None
    self._vad_output = None
    self._vad_frame_size = None
    self._vad_frame_size_ms = None
    self._c_attack = None
    self._c_decay = None

    self._vad_type = vad_type
    if self._vad_type not in self.VadType:
      raise exceptions.InitializationException(
          'Invalid vad type: ' + self._vad_type)
    logging.info('VAD used for annotations: ' + str(self._vad_type))

    assert os.path.exists(self._VAD_WEBRTC_BIN_PATH), self._VAD_WEBRTC_BIN_PATH

  @classmethod
  def GetOutputFileName(cls):
    return cls._OUTPUT_FILENAME

  def GetLevel(self):
    return self._level

  def GetLevelFrameSize(self):
    return self._level_frame_size

  @classmethod
  def GetLevelFrameSizeMs(cls):
    return cls._LEVEL_FRAME_SIZE_MS

  def GetVadOutput(self):
    return self._vad_output

  def GetVadFrameSize(self):
    return self._vad_frame_size

  def GetVadFrameSizeMs(self):
    return self._vad_frame_size_ms

  def Extract(self, filepath):
    # Load signal.
    self._signal = signal_processing.SignalProcessingUtils.LoadWav(filepath)
    if self._signal.channels != 1:
      raise NotImplementedError('multiple-channel annotations not implemented')

    # Level estimation params.
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

    # Ideal VAD output, it requires clean speech with high SNR as input.
    if self._vad_type == self.VadType.ENERGY_THRESHOLD:
      # Naive VAD based on level thresholding.
      vad_threshold = np.percentile(self._level, self._VAD_THRESHOLD)
      self._vad_output = np.uint8(self._level > vad_threshold)
      self._vad_frame_size = self._level_frame_size
      self._vad_frame_size_ms = self._LEVEL_FRAME_SIZE_MS
    elif self._vad_type == self.VadType.WEBRTC:
      # WebRTC VAD.
      self._RunWebRtcVad(filepath, self._signal.frame_rate)

  def Save(self, output_path):
    np.savez_compressed(
        file=os.path.join(output_path, self._OUTPUT_FILENAME),
        level=self._level,
        level_frame_size=self._level_frame_size,
        level_frame_size_ms=self._LEVEL_FRAME_SIZE_MS,
        vad_output=self._vad_output,
        vad_frame_size=self._vad_frame_size,
        vad_frame_size_ms=self._vad_frame_size_ms)

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

  def _RunWebRtcVad(self, wav_file_path, sample_rate):
    self._vad_output = None
    self._vad_frame_size = None

    # Create temporary output path.
    tmp_path = tempfile.mkdtemp()
    output_file_path = os.path.join(
        tmp_path, os.path.split(wav_file_path)[1] + '_vad.tmp')

    # Call WebRTC VAD.
    try:
      subprocess.call([
          self._VAD_WEBRTC_BIN_PATH,
          '-i', wav_file_path,
          '-o', output_file_path
      ], cwd=self._VAD_WEBRTC_PATH)

      # Read bytes.
      with open(output_file_path, 'rb') as f:
        raw_data = f.read()

      # Parse side information.
      self._vad_frame_size_ms = struct.unpack('B', raw_data[0])[0]
      self._vad_frame_size = self._vad_frame_size_ms * sample_rate / 1000
      assert self._vad_frame_size_ms in [10, 20, 30]
      extra_bits = struct.unpack('B', raw_data[-1])[0]
      assert 0 <= extra_bits <= 8

      # Init VAD vector.
      num_bytes = len(raw_data)
      num_frames = 8 * (num_bytes - 2) - extra_bits  # 8 frames for each byte.
      self._vad_output = np.zeros(num_frames, np.uint8)

      # Read VAD decisions.
      for i, byte in enumerate(raw_data[1:-1]):
        byte = struct.unpack('B', byte)[0]
        for j in range(8 if i < num_bytes - 3 else (8 - extra_bits)):
          self._vad_output[i * 8 + j] = int(byte & 1)
          byte = byte >> 1
    except Exception as e:
      logging.error('Error while running the WebRTC VAD (' + e.message + ')')
    finally:
      if os.path.exists(tmp_path):
        shutil.rmtree(tmp_path)
