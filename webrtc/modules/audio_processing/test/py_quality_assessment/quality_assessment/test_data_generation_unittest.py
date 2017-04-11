# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Unit tests for the test_data_generation module.
"""

import os
import shutil
import tempfile
import unittest

from . import test_data_generation
from . import test_data_generation_factory
from . import signal_processing


class TestTestDataGenerators(unittest.TestCase):
  """Unit tests for the test_data_generation module.
  """

  def setUp(self):
    """Create temporary folders."""
    self._base_output_path = tempfile.mkdtemp()
    self._input_noise_cache_path = tempfile.mkdtemp()

  def tearDown(self):
    """Recursively delete temporary folders."""
    shutil.rmtree(self._base_output_path)
    shutil.rmtree(self._input_noise_cache_path)

  def testTestDataGenerators(self):
    # Preliminary check.
    self.assertTrue(os.path.exists(self._base_output_path))
    self.assertTrue(os.path.exists(self._input_noise_cache_path))

    # Check that there is at least one registered test data generator.
    registered_classes = (
        test_data_generation.TestDataGenerator.REGISTERED_CLASSES)
    self.assertIsInstance(registered_classes, dict)
    self.assertGreater(len(registered_classes), 0)

    # Instance generators factory.
    generators_factory = (
        test_data_generation_factory.TestDataGeneratorFactory(
            aechen_ir_database_path=''))
    # TODO(alessiob): Replace with a mock of TestDataGeneratorFactory that
    # takes no arguments in the ctor. For those generators that need parameters,
    # it will return a mock generator (see the first comment in the next for
    # loop).

    # Use a sample input file as clean input signal.
    input_signal_filepath = os.path.join(
        os.getcwd(), 'probing_signals', 'tone-880.wav')
    self.assertTrue(os.path.exists(input_signal_filepath))

    # Load input signal.
    input_signal = signal_processing.SignalProcessingUtils.LoadWav(
        input_signal_filepath)

    # Try each registered test data generator.
    for generator_name in registered_classes:
      # Exclude ReverberationTestDataGenerator.
      # TODO(alessiob): Mock ReverberationTestDataGenerator, the mock
      # should rely on hard-coded impulse responses. This requires a mock for
      # TestDataGeneratorFactory. The latter knows whether returning the
      # actual generator or a mock object (as in the case of
      # ReverberationTestDataGenerator).
      if generator_name == (
          test_data_generation.ReverberationTestDataGenerator.NAME):
        continue

      # Instance test data generator.
      generator = generators_factory.GetInstance(
          registered_classes[generator_name])

      # Generate the noisy input - reference pairs.
      generator.Generate(
          input_signal_filepath=input_signal_filepath,
          input_noise_cache_path=self._input_noise_cache_path,
          base_output_path=self._base_output_path)

      # Perform checks.
      self._CheckGeneratedPairsListSizes(generator)
      self._CheckGeneratedPairsSignalDurations(generator, input_signal)
      self._CheckGeneratedPairsOutputPaths(generator)

  def _CheckGeneratedPairsListSizes(self, generator):
    config_names = generator.config_names
    number_of_pairs = len(config_names)
    self.assertEqual(number_of_pairs,
                     len(generator.noisy_signal_filepaths))
    self.assertEqual(number_of_pairs,
                     len(generator.apm_output_paths))
    self.assertEqual(number_of_pairs,
                     len(generator.reference_signal_filepaths))

  def _CheckGeneratedPairsSignalDurations(
      self, generator, input_signal):
    """Checks duration of the generated signals.

    Checks that the noisy input and the reference tracks are audio files
    with duration equal to or greater than that of the input signal.

    Args:
      generator: TestDataGenerator instance.
      input_signal: AudioSegment instance.
    """
    input_signal_length = (
        signal_processing.SignalProcessingUtils.CountSamples(input_signal))

    # Iterate over the noisy signal - reference pairs.
    for config_name in generator.config_names:
      # Load the noisy input file.
      noisy_signal_filepath = generator.noisy_signal_filepaths[
          config_name]
      noisy_signal = signal_processing.SignalProcessingUtils.LoadWav(
          noisy_signal_filepath)

      # Check noisy input signal length.
      noisy_signal_length = (
          signal_processing.SignalProcessingUtils.CountSamples(noisy_signal))
      self.assertGreaterEqual(noisy_signal_length, input_signal_length)

      # Load the reference file.
      reference_signal_filepath = generator.reference_signal_filepaths[
          config_name]
      reference_signal = signal_processing.SignalProcessingUtils.LoadWav(
          reference_signal_filepath)

      # Check noisy input signal length.
      reference_signal_length = (
          signal_processing.SignalProcessingUtils.CountSamples(
              reference_signal))
      self.assertGreaterEqual(reference_signal_length, input_signal_length)

  def _CheckGeneratedPairsOutputPaths(self, generator):
    """Checks that the output path created by the generator exists.

    Args:
      generator: TestDataGenerator instance.
    """
    # Iterate over the noisy signal - reference pairs.
    for config_name in generator.config_names:
      output_path = generator.apm_output_paths[config_name]
      self.assertTrue(os.path.exists(output_path))
