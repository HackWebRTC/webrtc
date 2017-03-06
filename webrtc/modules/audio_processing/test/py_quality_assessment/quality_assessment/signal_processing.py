# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import array
import logging

import numpy as np
import pydub
import scipy.signal

class SignalProcessingUtils(object):

  def __init__(self):
    pass

  @classmethod
  def load_wav(cls, filepath, channels=1):
    """
    Return:
      AudioSegment instance.
    """
    return pydub.AudioSegment.from_file(
        filepath, format='wav', channels=channels)

  @classmethod
  def save_wav(cls, output_filepath, signal):
    """
    Args:
      output_filepath: string, output file path.
      signal: AudioSegment instance.
    """
    return signal.export(output_filepath, format='wav')

  @classmethod
  def count_samples(cls, signal):
    """
    Number of samples per channel.

    Args:
      signal: AudioSegment instance.
    """
    number_of_samples = len(signal.get_array_of_samples())
    assert signal.channels > 0
    assert number_of_samples % signal.channels == 0
    return number_of_samples / signal.channels

  @classmethod
  def generate_white_noise(cls, signal):
    """
    Generate white noise with the same duration and in the same format as a
    given signal.

    Args:
      signal: AudioSegment instance.

    Return:
      AudioSegment instance.
    """
    generator = pydub.generators.WhiteNoise(
        sample_rate=signal.frame_rate,
        bit_depth=signal.sample_width * 8)
    return generator.to_audio_segment(
        duration=len(signal),
        volume=0.0)

  @classmethod
  def apply_impulse_response(cls, signal, impulse_response):
    # Get samples.
    assert signal.channels == 1, (
        'multiple-channel recordings not supported')
    samples = signal.get_array_of_samples()

    # Convolve.
    logging.info('applying %d order impulse response to a signal lasting %d ms',
                 len(impulse_response), len(signal))
    convolved_samples = scipy.signal.fftconvolve(
        in1=samples,
        in2=impulse_response,
        mode='full').astype(np.int16)
    logging.info('convolution computed')

    # Cast.
    convolved_samples = array.array(signal.array_type, convolved_samples)

    # Verify.
    logging.debug('signal length: %d samples', len(samples))
    logging.debug('convolved signal length: %d samples', len(convolved_samples))
    assert len(convolved_samples) > len(samples)

    # Generate convolved signal AudioSegment instance.
    convolved_signal = pydub.AudioSegment(
        data=convolved_samples,
        metadata={
            'sample_width': signal.sample_width,
            'frame_rate': signal.frame_rate,
            'frame_width': signal.frame_width,
            'channels': signal.channels,
        })
    assert len(convolved_signal) > len(signal)

    return convolved_signal

  @classmethod
  def normalize(cls, signal):
    return signal.apply_gain(-signal.max_dBFS)

  @classmethod
  def mix_signals(cls, signal_0, signal_1, target_snr=0.0,
                  bln_pad_shortest=False):
    """
    Mix two signals up to a desired SNR by scaling signal_0 (signal).

    Args:
      signal_0: AudioSegment instance (signal).
      signal_1: AudioSegment instance (noise).
      target_snr: float (dB).
      bln_pad_shortest: if True, it pads the shortest signal with silence at the
                        end.
    """
    # Pad signal_1 (if necessary). If signal_0 is the shortest, the AudioSegment
    # overlay() method implictly pads signal_0. Hence, the only case to handle
    # is signal_1 shorter than signal_0 and bln_pad_shortest True.
    if bln_pad_shortest:
      signal_0_duration = len(signal_0)
      signal_1_duration = len(signal_1)
      logging.debug('mix signals with padding')
      logging.debug('  signal_0: %d ms', signal_0_duration)
      logging.debug('  signal_1: %d ms', signal_1_duration)
      padding_duration = signal_0_duration - signal_1_duration
      if padding_duration > 0:  # That is signal_1_duration < signal_0_duration.
        logging.debug('  padding: %d ms', padding_duration)
        padding = pydub.AudioSegment.silent(
            duration=padding_duration,
            frame_rate=signal_0.frame_rate)
        logging.debug('  signal_1 (pre): %d ms', len(signal_1))
        signal_1 = signal_1 + padding
        logging.debug('  signal_1 (post): %d ms', len(signal_1))

    # Mix signals using the target SNR.
    power_0 = float(signal_0.dBFS)
    power_1 = float(signal_1.dBFS)
    gain_db = target_snr + power_1 - power_0
    return cls.normalize(signal_1.overlay(signal_0.apply_gain(gain_db)))
