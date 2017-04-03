# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""APM module simulator.
"""

import logging
import os

from . import audioproc_wrapper
from . import data_access
from . import eval_scores
from . import eval_scores_factory
from . import evaluation
from . import noise_generation
from . import noise_generation_factory


class ApmModuleSimulator(object):
  """APM module simulator class.
  """

  _NOISE_GENERATOR_CLASSES = noise_generation.NoiseGenerator.REGISTERED_CLASSES
  _EVAL_SCORE_WORKER_CLASSES = eval_scores.EvaluationScore.REGISTERED_CLASSES

  def __init__(self, aechen_ir_database_path, polqa_tool_path):
    # Init.
    self._audioproc_wrapper = audioproc_wrapper.AudioProcWrapper()
    self._evaluator = evaluation.ApmModuleEvaluator()

    # Instance factory objects.
    self._noise_generator_factory = (
        noise_generation_factory.NoiseGeneratorFactory(
            aechen_ir_database_path=aechen_ir_database_path))
    self._evaluation_score_factory = (
        eval_scores_factory.EvaluationScoreWorkerFactory(
            polqa_tool_path=polqa_tool_path))

    # Properties for each run.
    self._base_output_path = None
    self._noise_generators = None
    self._evaluation_score_workers = None
    self._config_filepaths = None
    self._input_filepaths = None

  def run(self, config_filepaths, input_filepaths, noise_generator_names,
          eval_score_names, output_dir):
    """
    Initializes paths and required instances, then runs all the simulations.
    """
    self._base_output_path = os.path.abspath(output_dir)

    # Instance noise generators.
    self._noise_generators = [self._noise_generator_factory.GetInstance(
        noise_generator_class=self._NOISE_GENERATOR_CLASSES[name]) for name in (
            noise_generator_names)]

    # Instance evaluation score workers.
    self._evaluation_score_workers = [
        self._evaluation_score_factory.GetInstance(
            evaluation_score_class=self._EVAL_SCORE_WORKER_CLASSES[name]) for (
                name) in eval_score_names]

    # Set APM configuration file paths.
    self._config_filepaths = self._get_paths_collection(config_filepaths)

    # Set probing signal file paths.
    self._input_filepaths = self._get_paths_collection(input_filepaths)

    self._simulate_all()

  def _simulate_all(self):
    """
    Iterates over the combinations of APM configurations, probing signals, and
    noise generators.
    """
    # Try different APM config files.
    for config_name in self._config_filepaths:
      config_filepath = self._config_filepaths[config_name]

      # Try different probing signal files.
      for input_name in self._input_filepaths:
        input_filepath = self._input_filepaths[input_name]

        # Try different noise generators.
        for noise_generator in self._noise_generators:
          logging.info('config: <%s>, input: <%s>, noise: <%s>',
                       config_name, input_name, noise_generator.NAME)

          # Output path for the input-noise pairs. It is used to cache the noisy
          # copies of the probing signals (shared across some simulations).
          input_noise_cache_path = os.path.join(
              self._base_output_path,
              '_cache',
              'input_{}-noise_{}'.format(input_name, noise_generator.NAME))
          data_access.make_directory(input_noise_cache_path)
          logging.debug('input-noise cache path: <%s>', input_noise_cache_path)

          # Full output path.
          output_path = os.path.join(
              self._base_output_path,
              'cfg-{}'.format(config_name),
              'input-{}'.format(input_name),
              'noise-{}'.format(noise_generator.NAME))
          data_access.make_directory(output_path)
          logging.debug('output path: <%s>', output_path)

          self._simulate(noise_generator, input_filepath,
                         input_noise_cache_path, output_path, config_filepath)

  def _simulate(self, noise_generator, input_filepath, input_noise_cache_path,
                output_path, config_filepath):
    """
    Simulates a given combination of APM configurations, probing signals, and
    noise generators. It iterates over the noise generator internal
    configurations.
    """
    # Generate pairs of noisy input and reference signal files.
    noise_generator.generate(
        input_signal_filepath=input_filepath,
        input_noise_cache_path=input_noise_cache_path,
        base_output_path=output_path)

    # For each input-reference pair, simulate a call and evaluate.
    for noise_generator_config_name in noise_generator.config_names:
      logging.info(' - noise config: <%s>', noise_generator_config_name)

      # APM input and output signal paths.
      noisy_signal_filepath = noise_generator.noisy_signal_filepaths[
          noise_generator_config_name]
      evaluation_output_path = noise_generator.output_paths[
          noise_generator_config_name]

      # Simulate a call using the audio processing module.
      self._audioproc_wrapper.run(
          config_filepath=config_filepath,
          input_filepath=noisy_signal_filepath,
          output_path=evaluation_output_path)

      # Reference signal path for the evaluation step.
      reference_signal_filepath = noise_generator.reference_signal_filepaths[
          noise_generator_config_name]

      # Evaluate.
      self._evaluator.run(
          evaluation_score_workers=self._evaluation_score_workers,
          apm_output_filepath=self._audioproc_wrapper.output_filepath,
          reference_input_filepath=reference_signal_filepath,
          output_path=evaluation_output_path)

  @classmethod
  def _get_paths_collection(cls, filepaths):
    """
    Given a list of file paths, makes a collection with one pair for each item
    in the list where the key is the file name without extension and the value
    is the path.
    """
    filepaths_collection = {}
    for filepath in filepaths:
      name = os.path.splitext(os.path.split(filepath)[1])[0]
      filepaths_collection[name] = os.path.abspath(filepath)
    return filepaths_collection
