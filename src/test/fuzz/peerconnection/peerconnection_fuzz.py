#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

import random
import string
import unicodedata

from common.fuzz_parameters import FillInParameter
from common.random_javascript import *


def ArrayOfRandomRolls(num_rolls):
  return str([random.random() for x in xrange(num_rolls)])


def RandomAudioOrVideo():
  roll = random.random()
  if roll < 0.5:
    return '{ video: true, audio:true }'
  elif roll < 0.7:
    return '{ video: true, audio: false }'
  elif roll < 0.9:
    return '{ video: false, audio: true }'
  else:
    return '{ video: false, audio: true }'


def ReturnFirstArgument():
  return 'function(arg) { return arg; }'


def ReturnRandomUtf8String():
  unicode_glyphs = ''.join(unichr(char) for char in xrange(0x10ffff + 1)
    if unicodedata.category(unichr(char))[0] in ('LMNPSZ'))
  oh_dear = random.sample(unicode_glyphs, random.randint(50, 1500))
  return 'function(arg) { return "%s"; }' % ''.join(oh_dear)


def ReturnFuzzedSdp():
  return 'function(arg) { return fuzzSdp(arg); }'


def RandomSdpTransform():
  roll = random.random()
  if roll < 0.1:
    return ReturnRandomUtf8String()
  elif roll < 0.5:
    return ReturnFuzzedSdp()
  else:
    return ReturnFirstArgument()


def Fuzz(file_data):
  file_data = file_data.decode('utf-8')

  # Generate a bunch of random numbers and encode them into the page. Since the
  # values get hard-coded into the page the page's choices will be reproducible.
  file_data = FillInParameter('ARRAY_OF_RANDOM_ROLLS',
                              ArrayOfRandomRolls(500),
                              file_data)
  file_data = FillInParameter('REQUEST_AUDIO_AND_VIDEO',
                              RandomAudioOrVideo(),
                              file_data)
  file_data = FillInParameter('TRANSFORM_OFFER_SDP',
                              RandomSdpTransform(),
                              file_data)
  file_data = FillInParameter('TRANSFORM_ANSWER_SDP',
                              RandomSdpTransform(),
                              file_data)

  return file_data