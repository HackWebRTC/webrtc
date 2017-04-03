# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Evaluation score abstract class and implementations.
"""

import logging
import os
import re
import subprocess

from . import data_access
from . import exceptions
from . import signal_processing


class EvaluationScore(object):

  NAME = None
  REGISTERED_CLASSES = {}

  def __init__(self):
    self._reference_signal = None
    self._reference_signal_filepath = None
    self._tested_signal = None
    self._tested_signal_filepath = None
    self._output_filepath = None
    self._score = None

  @classmethod
  def register_class(cls, class_to_register):
    """Register an EvaluationScore implementation.

    Decorator to automatically register the classes that extend EvaluationScore.
    """
    cls.REGISTERED_CLASSES[class_to_register.NAME] = class_to_register
    return class_to_register

  @property
  def output_filepath(self):
    return self._output_filepath

  @property
  def score(self):
    return self._score

  def set_reference_signal_filepath(self, filepath):
    """ Set the path to the audio track used as reference signal.
    """
    self._reference_signal_filepath = filepath

  def set_tested_signal_filepath(self, filepath):
    """ Set the path to the audio track used as test signal.
    """
    self._tested_signal_filepath = filepath

  def _load_reference_signal(self):
    assert self._reference_signal_filepath is not None
    self._reference_signal = signal_processing.SignalProcessingUtils.load_wav(
        self._reference_signal_filepath)

  def _load_tested_signal(self):
    assert self._tested_signal_filepath is not None
    self._tested_signal = signal_processing.SignalProcessingUtils.load_wav(
        self._tested_signal_filepath)

  def run(self, output_path):
    """Extracts the score for the set input-reference pair.
    """
    self._output_filepath = os.path.join(output_path, 'score-{}.txt'.format(
        self.NAME))
    try:
      # If the score has already been computed, load.
      self._load_score()
      logging.debug('score found and loaded')
    except IOError:
      # Compute the score.
      logging.debug('score not found, compute')
      self._run(output_path)

  def _run(self, output_path):
    # Abstract method.
    raise NotImplementedError()

  def _load_score(self):
    return data_access.ScoreFile.load(self._output_filepath)

  def _save_score(self):
    return data_access.ScoreFile.save(self._output_filepath, self._score)


@EvaluationScore.register_class
class AudioLevelScore(EvaluationScore):
  """Compute the difference between the average audio level of the tested and
  the reference signals.

  Unit: dB
  Ideal: 0 dB
  Worst case: +/-inf dB
  """

  NAME = 'audio_level'

  def __init__(self):
    EvaluationScore.__init__(self)

  def _run(self, output_path):
    self._load_reference_signal()
    self._load_tested_signal()
    self._score = self._tested_signal.dBFS - self._reference_signal.dBFS
    self._save_score()


@EvaluationScore.register_class
class PolqaScore(EvaluationScore):
  """Compute the POLQA score.

  Unit: MOS
  Ideal: 4.5
  Worst case: 1.0
  """

  NAME = 'polqa'
  _BIN_FILENAME = 'PolqaOem64'

  def __init__(self, polqa_tool_path):
    EvaluationScore.__init__(self)

    # Path to the POLQA directory with binary and license files.
    self._polqa_tool_path = polqa_tool_path

    # POLQA binary file path.
    self._polqa_bin_filepath = os.path.join(
        self._polqa_tool_path, self._BIN_FILENAME)
    if not os.path.exists(self._polqa_bin_filepath):
      logging.error('cannot find POLQA tool binary file')
      raise exceptions.FileNotFoundError()

  def _run(self, output_path):
    polqa_out_filepath = os.path.join(output_path, 'polqa.out')
    if os.path.exists(polqa_out_filepath):
      os.unlink(polqa_out_filepath)

    args = [
        self._polqa_bin_filepath, '-t', '-q', '-Overwrite',
        '-Ref', self._reference_signal_filepath,
        '-Test', self._tested_signal_filepath,
        '-LC', 'NB',
        '-Out', polqa_out_filepath,
    ]
    logging.debug(' '.join(args))
    subprocess.call(args, cwd=self._polqa_tool_path)

    # Parse POLQA tool output and extract the score.
    polqa_output = self._parse_output_file(polqa_out_filepath)
    self._score = float(polqa_output['PolqaScore'])

    self._save_score()

  @classmethod
  def _parse_output_file(cls, polqa_out_filepath):
    """
    Parse the POLQA tool output formatted as a table ('-t' option).
    """
    data = []
    with open(polqa_out_filepath) as f:
      for line in f:
        line = line.strip()
        if len(line) == 0 or line.startswith('*'):
          # Ignore comments.
          continue
        # Read fields.
        data.append(re.split(r'\t+', line))

    # Two rows expected (header and values).
    assert len(data) == 2, 'Cannot parse POLQA output'
    number_of_fields = len(data[0])
    assert number_of_fields == len(data[1])

    # Build and return a dictionary with field names (header) as keys and the
    # corresponding field values as values.
    return {data[0][index]: data[1][index] for index in range(number_of_fields)}
