# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Input signal creator module.
"""

from . import exceptions
from . import signal_processing


class InputSignalCreator(object):
  """Input signal creator class.
  """

  @classmethod
  def Create(cls, name, params):
    """Creates a input signal.

    Args:
      name: Input signal creator name.
      params: Tuple of parameters to pass to the specific signal creator.

    Returns:
      AudioSegment instance.
    """
    try:
      if name == 'pure_tone':
        return cls._CreatePureTone(float(params[0]), int(params[1]))
    except (TypeError, AssertionError) as e:
      raise exceptions.InputSignalCreatorException(
          'Invalid signal creator parameters: {}'.format(e))

    raise exceptions.InputSignalCreatorException(
        'Invalid input signal creator name')

  @classmethod
  def _CreatePureTone(cls, frequency, duration):
    """
    Generates a pure tone at 48000 Hz.

    Args:
      frequency: Float in (0-24000] (Hz).
      duration: Integer (milliseconds).

    Returns:
      AudioSegment instance.
    """
    assert 0 < frequency <= 24000
    assert 0 < duration
    template = signal_processing.SignalProcessingUtils.GenerateSilence(duration)
    return signal_processing.SignalProcessingUtils.GeneratePureTone(
        template, frequency)
