# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import os
import shutil
import tempfile
import unittest

from . import noise_generation
from . import signal_processing

class TestNoiseGen(unittest.TestCase):

  def setUp(self):
    """Create temporary folders."""
    self._base_output_path = tempfile.mkdtemp()
    self._input_noise_cache_path = tempfile.mkdtemp()

  def tearDown(self):
    """Recursively delete temporary folders."""
    shutil.rmtree(self._base_output_path)
    shutil.rmtree(self._input_noise_cache_path)

  def testNoiseGenerators(self):
    # Preliminary check.
    self.assertTrue(os.path.exists(self._base_output_path))
    self.assertTrue(os.path.exists(self._input_noise_cache_path))

    # Check that there is at least one registered noise generator.
    registered_classes = noise_generation.NoiseGenerator.REGISTERED_CLASSES
    self.assertIsInstance(registered_classes, dict)
    self.assertGreater(len(registered_classes), 0)

    # Use a sample input file as clean input signal.
    input_signal_filepath = os.path.join(
        os.getcwd(), 'probing_signals', 'tone-880.wav')
    self.assertTrue(os.path.exists(input_signal_filepath))

    # Load input signal.
    input_signal = signal_processing.SignalProcessingUtils.load_wav(
        input_signal_filepath)

    # Try each registered noise generator.
    for noise_generator_name in registered_classes:
      # Instance noise generator.
      noise_generator_class = registered_classes[noise_generator_name]
      noise_generator = noise_generator_class()

      # Generate the noisy input - reference pairs.
      noise_generator.generate(
          input_signal_filepath=input_signal_filepath,
          input_noise_cache_path=self._input_noise_cache_path,
          base_output_path=self._base_output_path)

      # Perform checks.
      self._CheckNoiseGeneratorPairsListSizes(noise_generator)
      self._CheckNoiseGeneratorPairsSignalDurations(
          noise_generator, input_signal)
      self._CheckNoiseGeneratorPairsOutputPaths(noise_generator)

  def _CheckNoiseGeneratorPairsListSizes(self, noise_generator):
    # Noise configuration names.
    noise_config_names = noise_generator.config_names
    number_of_pairs = len(noise_config_names)

    # Check.
    self.assertEqual(number_of_pairs,
                     len(noise_generator.noisy_signal_filepaths))
    self.assertEqual(number_of_pairs,
                     len(noise_generator.output_paths))
    self.assertEqual(number_of_pairs,
                     len(noise_generator.reference_signal_filepaths))

  def _CheckNoiseGeneratorPairsSignalDurations(
      self, noise_generator, input_signal):
    """Checks that the noisy input and the reference tracks are audio files
       with duration >= to that of the input signal.
    """
    input_signal_length = (
        signal_processing.SignalProcessingUtils.count_samples(input_signal))

    # Iterate over the noisy signal - reference pairs.
    for noise_config_name in noise_generator.config_names:
      # Load the noisy input file.
      noisy_signal_filepath = noise_generator.noisy_signal_filepaths[
          noise_config_name]
      noisy_signal = signal_processing.SignalProcessingUtils.load_wav(
          noisy_signal_filepath)

      # Check noisy input signal length.
      noisy_signal_length = (
          signal_processing.SignalProcessingUtils.count_samples(noisy_signal))
      self.assertGreaterEqual(noisy_signal_length, input_signal_length)

      # Load the reference file.
      reference_signal_filepath = (
          noise_generator.reference_signal_filepaths[noise_config_name])
      reference_signal = signal_processing.SignalProcessingUtils.load_wav(
          reference_signal_filepath)

      # Check noisy input signal length.
      reference_signal_length = (
          signal_processing.SignalProcessingUtils.count_samples(
              reference_signal))
      self.assertGreaterEqual(reference_signal_length, input_signal_length)

  def _CheckNoiseGeneratorPairsOutputPaths(self, noise_generator):
    """Checks that the output path created by the generator exists.
    """
    # Iterate over the noisy signal - reference pairs.
    for noise_config_name in noise_generator.config_names:
      output_path = noise_generator.output_paths[noise_config_name]
      self.assertTrue(os.path.exists(output_path))
