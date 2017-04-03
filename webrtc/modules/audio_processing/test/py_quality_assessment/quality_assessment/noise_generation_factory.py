# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""NoiseGenerator factory class.
"""

import logging

from . import noise_generation


class NoiseGeneratorFactory(object):
  """Factory class used to instantiate noise generator workers.

     It can be used by instanciating a factory, passing parameters to the
     constructor. These parameters are used to instantiate noise generator
     workers.
  """

  def __init__(self, aechen_ir_database_path):
    self._aechen_ir_database_path = aechen_ir_database_path

  def GetInstance(self, noise_generator_class):
    """Creates an NoiseGenerator instance given a class object.
    """
    logging.debug(
        'factory producing a %s noise generator', noise_generator_class)
    if noise_generator_class == noise_generation.EchoNoiseGenerator:
      return noise_generation.EchoNoiseGenerator(self._aechen_ir_database_path)
    else:
      # By default, no arguments in the constructor.
      return noise_generator_class()
