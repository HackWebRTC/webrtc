# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

class EvaluationScore(object):

  NAME = None
  REGISTERED_CLASSES = {}

  def __init__(self):
    pass

  @classmethod
  def register_class(cls, class_to_register):
    """
    Decorator to automatically register the classes that extend EvaluationScore.
    """
    cls.REGISTERED_CLASSES[class_to_register.NAME] = class_to_register


@EvaluationScore.register_class
class AudioLevelScore(EvaluationScore):
  """
  Compute the difference between the average audio level of the tested and
  the reference signals.

  Unit: dB
  Ideal: 0 dB
  Worst case: +/-inf dB
  """

  NAME = 'audio_level'

  def __init__(self):
    super(AudioLevelScore, self).__init__()


@EvaluationScore.register_class
class PolqaScore(EvaluationScore):
  """
  Compute the POLQA score. It requires that the POLQA_PATH environment variable
  points to the PolqaOem64 executable.

  Unit: MOS
  Ideal: 4.5
  Worst case: 1.0
  """

  NAME = 'polqa'

  def __init__(self):
    super(PolqaScore, self).__init__()
