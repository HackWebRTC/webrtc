# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import os

from . import data_access
from .signal_processing import SignalProcessingUtils

class NoiseGenerator(object):
  """Abstract class responsible for the generation of noisy signals.

  Given a clean signal, it generates two streams named noisy signal and
  reference.  The former is the clean signal deteriorated by the noise source,
  the latter goes trhough the same deterioration process, but more "gently".
  Noisy signal and reference are produced so that the reference is the signal
  expected at the output of the APM module when the latter is fed with the nosiy
  signal.

  This is useful since it is not realistic to expect that APM will remove all
  the background noise or all the echo. Hence, the process that generates the
  reference signal is responsible for setting realistic expectations.

  Finally, note that a noise source can generate multiple input-reference pairs.
  """

  NAME = None
  REGISTERED_CLASSES = {}

  def __init__(self):
    # Input
    self._noisy_signal_filepaths = None
    self._output_paths = None
    self._reference_signal_filepaths = None
    self.clear()

  @classmethod
  def register_class(cls, class_to_register):
    """ Decorator to automatically register the classes that extend
        NoiseGenerator.
    """
    cls.REGISTERED_CLASSES[class_to_register.NAME] = class_to_register

  @property
  def config_names(self):
    return self._noisy_signal_filepaths.keys()

  @property
  def noisy_signal_filepaths(self):
    return self._noisy_signal_filepaths

  @property
  def output_paths(self):
    return self._output_paths

  @property
  def reference_signal_filepaths(self):
    return self._reference_signal_filepaths

  def generate(
      self, input_signal_filepath, input_noise_cache_path, base_output_path):
    self.clear()
    return self._generate(
        input_signal_filepath, input_noise_cache_path, base_output_path)

  def clear(self):
    self._noisy_signal_filepaths = {}
    self._output_paths = {}
    self._reference_signal_filepaths = {}

  def _generate(
      self, input_signal_filepath, input_noise_cache_path, base_output_path):
    raise NotImplementedError()

  def _add_noise_snr_pairs(self, base_output_path, noisy_mix_filepaths,
                           snr_value_pairs):
    """ Add noisy-reference signal pairs.

    Args:
      base_output_path: noisy tracks base output path.
      noisy_mix_filepaths: nested dictionary of noisy signal paths organized
                           by noisy track name and SNR level.
      snr_value_pairs: list of SNR pairs.
    """
    for noise_track_name in noisy_mix_filepaths:
      for snr_noisy, snr_refence in snr_value_pairs:
        config_name = '{0}_{1:d}_{2:d}_SNR'.format(
            noise_track_name, snr_noisy, snr_refence)
        output_path = self._make_dir(base_output_path, config_name)
        self._add_noise_reference_files_pair(
            config_name=config_name,
            noisy_signal_filepath=noisy_mix_filepaths[
                noise_track_name][snr_noisy],
            reference_signal_filepath=noisy_mix_filepaths[
                noise_track_name][snr_refence],
            output_path=output_path)

  def _add_noise_reference_files_pair(self, config_name, noisy_signal_filepath,
                                      reference_signal_filepath, output_path):
    assert config_name not in self._noisy_signal_filepaths
    self._noisy_signal_filepaths[config_name] = os.path.abspath(
        noisy_signal_filepath)
    self._output_paths[config_name] = os.path.abspath(output_path)
    self._reference_signal_filepaths[config_name] = os.path.abspath(
        reference_signal_filepath)

    # Save noisy and reference file paths.
    data_access.Metadata.save_audio_in_ref_paths(
        output_path=output_path,
        audio_in_filepath=self._noisy_signal_filepaths[config_name],
        audio_ref_filepath=self._reference_signal_filepaths[config_name])

  @classmethod
  def _make_dir(cls, base_output_path, noise_generator_config_name):
    output_path = os.path.join(base_output_path, noise_generator_config_name)
    data_access.make_directory(output_path)
    return output_path


# Identity generator.
@NoiseGenerator.register_class
class IdentityGenerator(NoiseGenerator):
  """
  Generator that adds no noise, therefore both the noisy and the reference
  signals are the input signal.
  """

  NAME = 'identity'

  def __init__(self):
    NoiseGenerator.__init__(self)

  def _generate(
      self, input_signal_filepath, input_noise_cache_path, base_output_path):
    CONFIG_NAME = 'default'
    output_path = self._make_dir(base_output_path, CONFIG_NAME)
    self._add_noise_reference_files_pair(
        config_name=CONFIG_NAME,
        noisy_signal_filepath=input_signal_filepath,
        reference_signal_filepath=input_signal_filepath,
        output_path=output_path)


@NoiseGenerator.register_class
class WhiteNoiseGenerator(NoiseGenerator):
  """
  Additive white noise generator.
  """

  NAME = 'white'

  # Each pair indicates the clean vs. noisy and reference vs. noisy SNRs.
  # Since the implementation below only changes the gain of the noise, the
  # values indicate the noise-to-signal ratio. Therefore a higher value means
  # larger amount of noise.
  # The reference (second value of each pair) always has a lower amount of noise
  # - i.e., the SNR is 10 dB higher.
  _SNR_VALUE_PAIRS = [
      [0, -10],  # Largest noise.
      [-5, -15],
      [-10, -20],
      [-20, -30],  # Smallest noise.
  ]

  _NOISY_SIGNAL_FILENAME_TEMPLATE = 'noise_{0:d}_SNR.wav'

  def __init__(self):
    NoiseGenerator.__init__(self)

  def _generate(
      self, input_signal_filepath, input_noise_cache_path, base_output_path):
    # Load the input signal.
    input_signal = SignalProcessingUtils.load_wav(input_signal_filepath)
    input_signal = SignalProcessingUtils.normalize(input_signal)

    # Create the noise track.
    noise_signal = SignalProcessingUtils.generate_white_noise(input_signal)
    noise_signal = SignalProcessingUtils.normalize(noise_signal)

    # Create the noisy mixes (once for each unique SNR value).
    noisy_mix_filepaths = {}
    snr_values = set([snr for pair in self._SNR_VALUE_PAIRS for snr in pair])
    for snr in snr_values:
      noisy_signal_filepath = os.path.join(
          input_noise_cache_path,
          self._NOISY_SIGNAL_FILENAME_TEMPLATE.format(snr))

      # Create and save if not done.
      if not os.path.exists(noisy_signal_filepath):
        # Create noisy signal.
        noisy_signal = SignalProcessingUtils.mix_signals(
            noise_signal, input_signal, snr)

        # Save.
        SignalProcessingUtils.save_wav(noisy_signal_filepath, noisy_signal)

      # Add file to the collection of mixes.
      noisy_mix_filepaths[snr] = noisy_signal_filepath

    # Add all the noisy-reference signal pairs.
    for snr_noisy, snr_refence in self._SNR_VALUE_PAIRS:
      config_name = '{0:d}_{1:d}_SNR'.format(snr_noisy, snr_refence)
      output_path = self._make_dir(base_output_path, config_name)
      self._add_noise_reference_files_pair(
          config_name=config_name,
          noisy_signal_filepath=noisy_mix_filepaths[snr_noisy],
          reference_signal_filepath=noisy_mix_filepaths[snr_refence],
          output_path=output_path)


# TODO(alessiob): remove comment when class implemented.
# @NoiseGenerator.register_class
class NarrowBandNoiseGenerator(NoiseGenerator):
  """
  Additive narrow-band noise generator.
  """

  NAME = 'narrow_band'

  def __init__(self):
    NoiseGenerator.__init__(self)

  def _generate(
      self, input_signal_filepath, input_noise_cache_path, base_output_path):
    # TODO(alessiob): implement.
    pass


# TODO(alessiob): remove comment when class implemented.
# @NoiseGenerator.register_class
class EnvironmentalNoiseGenerator(NoiseGenerator):
  """
  Additive environmental noise generator.
  """

  NAME = 'environmental'

  def __init__(self):
    NoiseGenerator.__init__(self)

  def _generate(
      self, input_signal_filepath, input_noise_cache_path, base_output_path):
    # TODO(alessiob): implement.
    pass


# TODO(alessiob): remove comment when class implemented.
# @NoiseGenerator.register_class
class EchoNoiseGenerator(NoiseGenerator):
  """
  Echo noise generator.
  """

  NAME = 'echo'

  def __init__(self):
    NoiseGenerator.__init__(self)

  def _generate(
      self, input_signal_filepath, input_noise_cache_path, base_output_path):
    # TODO(alessiob): implement.
    pass
